#include <vk_scene.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include "vk_mem_alloc.h"
#include <future>

void RenderScene::uploadObjectData(VkCommandBuffer cmd, VulkanEngine* engine)
{
	std::vector<VkBufferMemoryBarrier> uploadBarriers;

	/*
	*   Upload object data to GPU.
	*/
	if (dirtyRenderObjects.size() > 0)
	{
		size_t copySize = allRenderObjects.size() * sizeof(GPUObjectData);
		if (!objectDataBuffer.has_value() || objectDataBuffer->info.size < copySize)
		{
			objectDataBuffer = engine->createBuffer(copySize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			engine->_mainDeletionQueue.push_function([=]() {
				engine->destroyBuffer(objectDataBuffer.value());
				});
		}

		AllocatedBuffer newBuffer = engine->createBuffer(copySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		GPUObjectData* objectSSBO = (GPUObjectData*)engine->mapBuffer(newBuffer);
		for (int i = 0; i < allRenderObjects.size(); i++)
		{
			Handle<RenderObject> objHandle;
			objHandle.handle = i;
			RenderObject* obj = getRenderObject(objHandle);

			GPUObjectData objectData;
			objectData.transform = obj->transform;
			objectData.boundOriginAndRadius = glm::vec4(obj->bounds.origin, obj->bounds.sphereRadius);
			objectData.boundExtent = glm::vec4(obj->bounds.extents, 0);

			memcpy(objectSSBO + i, &objectData, sizeof(GPUObjectData));
		}
		engine->unmapBuffer(newBuffer);

		//copy from the uploaded cpu side instance buffer to the gpu one
		VkBufferCopy bufferCopy;
		bufferCopy.srcOffset = 0;
		bufferCopy.dstOffset = 0;
		bufferCopy.size = allRenderObjects.size() * sizeof(GPUObjectData);
		vkCmdCopyBuffer(cmd, newBuffer.buffer, objectDataBuffer.value().buffer, 1, &bufferCopy);

		VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(objectDataBuffer.value().buffer, engine->_graphicsQueueFamily, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

		engine->destroyBuffer(newBuffer);

		uploadBarriers.emplace_back(barrier);
		clearDirtyObjects();
	}

	/*
	*  Create indirect buffer on GPU.
	*/
	//TBD MeshPass* passes[3] = { &shadowPass, &forwardOpaquePass, &forwardTransparentPass };
	MeshPass* passes[2] = { &forwardOpaquePass, &forwardTransparentPass };
	for (int i = 0; i < 2; i++)
	{
		MeshPass* pass = passes[i];

		if (!pass->drawIndirectBuffer.has_value() || pass->drawIndirectBuffer.value().info.size < pass->indirectBatches.size() * sizeof(GPUIndirectObject))
		{
			pass->drawIndirectBuffer = engine->createBuffer(pass->indirectBatches.size() * sizeof(GPUIndirectObject), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				engine->destroyBuffer(pass->drawIndirectBuffer.value());
				});
		}

		if (!pass->compactedInstanceBuffer.has_value() || pass->compactedInstanceBuffer.value().info.size < pass->flatBatches.size() * sizeof(uint32_t))
		{
			pass->compactedInstanceBuffer = engine->createBuffer(pass->flatBatches.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				engine->destroyBuffer(pass->compactedInstanceBuffer.value());
				});
		}

		if (!pass->GPUInstanceBuffer.has_value() || pass->GPUInstanceBuffer.value().info.size < pass->flatBatches.size() * sizeof(GPUInstance))
		{
			pass->GPUInstanceBuffer = engine->createBuffer(pass->flatBatches.size() * sizeof(GPUInstance), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				engine->destroyBuffer(pass->GPUInstanceBuffer.value());
				});
		}
	}

	/*
	*   Upload data to buffers above.
	*/
	for (int i = 0; i < 2; i++)
	{
		MeshPass* pass = passes[i];

		if (pass->needsIndirectRefresh && pass->indirectBatches.size() > 0)
		{
			AllocatedBuffer newIndirectBuffer = engine->createBuffer(pass->indirectBatches.size() * sizeof(GPUIndirectObject), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			GPUIndirectObject* indirectData = (GPUIndirectObject*)engine->mapBuffer(newIndirectBuffer);
			for (int i = 0; i < pass->indirectBatches.size(); i++) {

				IndirectBatch indirectBatch = pass->indirectBatches[i];

				indirectData[i].command.firstInstance = indirectBatch.first;;
				indirectData[i].command.instanceCount = 0;
				indirectData[i].command.firstIndex = getMesh(indirectBatch.meshID)->firstIndex;
				indirectData[i].command.vertexOffset = getMesh(indirectBatch.meshID)->firstVertex;
				indirectData[i].command.indexCount = getMesh(indirectBatch.meshID)->indexCount;
				indirectData[i].objectID = 0;
				indirectData[i].batchID = i;
			}
			engine->unmapBuffer(newIndirectBuffer);
			engine->getCurrentFrame()._deletionQueue.push_function([=]() {
				engine->destroyBuffer(newIndirectBuffer);
				});
			
			pass->clearIndirectBuffer = std::move(newIndirectBuffer);
			pass->needsIndirectRefresh = false;
		}

		if (pass->needsInstanceRefresh && pass->flatBatches.size() > 0)
		{
			AllocatedBuffer newInstanceBuffer = engine->createBuffer(pass->flatBatches.size() * sizeof(GPUInstance), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			GPUInstance* instanceData = (GPUInstance*)engine->mapBuffer(newInstanceBuffer);

			int dataIndex = 0;
			for (int i = 0; i < pass->indirectBatches.size(); i++) {
				IndirectBatch indirectBatch = pass->indirectBatches[i];

				for (int flatBatchIndex = 0; flatBatchIndex < indirectBatch.count; flatBatchIndex++)
				{
					instanceData[dataIndex].objectID = pass->get(pass->flatBatches[indirectBatch.first + flatBatchIndex].object)->original.handle;
					instanceData[dataIndex].batchID = i;
					dataIndex++;
				}
			}

			engine->unmapBuffer(newInstanceBuffer);
			engine->getCurrentFrame()._deletionQueue.push_function([=]() {
				engine->destroyBuffer(newInstanceBuffer);
				});

			VkBufferCopy instanceCopy;
			instanceCopy.srcOffset = 0;
			instanceCopy.dstOffset = 0;
			instanceCopy.size = pass->flatBatches.size() * sizeof(GPUInstance);
			vkCmdCopyBuffer(cmd, newInstanceBuffer.buffer, pass->GPUInstanceBuffer.value().buffer, 1, &instanceCopy);

			pass->needsInstanceRefresh = false;

			VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(pass->GPUInstanceBuffer.value().buffer, engine->_graphicsQueueFamily, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

			uploadBarriers.push_back(barrier);
		}
	}

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(uploadBarriers.size()), uploadBarriers.data(), 0, nullptr);

	uploadBarriers.clear();
}

void RenderScene::updateObject(Handle<RenderObject> objectID)
{
	auto& passIndices = getRenderObject(objectID)->passIndices;
	if (passIndices[MaterialPass::forwardOpaque] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MaterialPass::forwardOpaque];

		forwardOpaquePass.objectsToDelete.push_back(obj);
		forwardOpaquePass.unbatchedObjects.push_back(objectID);

		passIndices[MaterialPass::forwardOpaque] = -1;
	}

	if (passIndices[MaterialPass::shadow] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MaterialPass::shadow];

		shadowPass.objectsToDelete.push_back(obj);
		shadowPass.unbatchedObjects.push_back(objectID);

		passIndices[MaterialPass::shadow] = -1;
	}

	if (passIndices[MaterialPass::forwardTransparent] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MaterialPass::forwardTransparent];

		forwardTransparentPass.objectsToDelete.push_back(obj);
		forwardTransparentPass.unbatchedObjects.push_back(objectID);
		
		passIndices[MaterialPass::forwardTransparent] = -1;
	}


	if (getRenderObject(objectID)->updateIndex == (uint32_t)-1)
	{
		getRenderObject(objectID)->updateIndex = static_cast<uint32_t>(dirtyRenderObjects.size());

		dirtyRenderObjects.push_back(objectID);
	}
}

RenderObject* RenderScene::getRenderObject(Handle<RenderObject> objectID)
{
	return &allRenderObjects[objectID.handle];
}

Handle<DrawMesh> RenderScene::getMeshHandle(const GeoSurface& surface, std::shared_ptr<MeshAsset> meshAsset)
{
	Handle<DrawMesh> handle;
	auto it = convertedMesh.find(meshAsset.get());
	if (it == convertedMesh.end())
	{
		uint32_t index = static_cast<uint32_t>(drawMeshes.size());

		DrawMesh newMesh;
		newMesh.meshAsset = meshAsset;
		newMesh.firstIndex = surface.startIndex;
		newMesh.firstVertex = surface.startVertex;
		newMesh.vertexCount = static_cast<uint32_t>(meshAsset->meshBuffers.original->_vertices.size());
		newMesh.indexCount = static_cast<uint32_t>(meshAsset->meshBuffers.original->_indices.size());

		drawMeshes.push_back(newMesh);

		handle.handle = index;
		convertedMesh[meshAsset.get()] = handle;
	}
	else {
		handle = (*it).second;
	}
	return handle;
}

void RenderScene::mergeMeshes(VulkanEngine* engine)
{
	uint32_t totalVertices = 0;
	uint32_t totalIndices = 0;

	for(DrawMesh& mesh: drawMeshes)
	{
		mesh.firstVertex = totalVertices;
		mesh.firstIndex = totalIndices;

		totalVertices += mesh.vertexCount;
		totalIndices += mesh.indexCount;

		mesh.isMerged = true;
	}

	mergedVertexBuffer = engine->createBuffer(totalVertices * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	mergedIndexBuffer = engine->createBuffer(totalVertices * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	engine->immediateSubmit([&](VkCommandBuffer cmd) {
		for (DrawMesh& mesh : drawMeshes)
		{
			VkBufferCopy vertexCopy;
			vertexCopy.dstOffset = mesh.firstVertex * sizeof(Vertex);
			vertexCopy.size = mesh.vertexCount * sizeof(Vertex);
			vertexCopy.srcOffset = 0;

			vkCmdCopyBuffer(cmd, mesh.meshAsset.lock()->meshBuffers.vertexBuffer.buffer, mergedVertexBuffer->buffer, 1, &vertexCopy);

			VkBufferCopy indexCopy;
			indexCopy.dstOffset = mesh.firstIndex * sizeof(uint32_t);
			indexCopy.size = mesh.indexCount * sizeof(uint32_t);
			indexCopy.srcOffset = 0;

			vkCmdCopyBuffer(cmd, mesh.meshAsset.lock()->meshBuffers.indexBuffer.buffer, mergedIndexBuffer->buffer, 1, &indexCopy);
		}
		});

	engine->_mainDeletionQueue.push_function([=]() {
		engine->destroyBuffer(mergedVertexBuffer.value());
		engine->destroyBuffer(mergedIndexBuffer.value());
		});
}

void RenderScene::buildBatches()
{
	//refreshPass(&shadowPass);
	refreshPass(&forwardOpaquePass);
	refreshPass(&forwardTransparentPass);
}

void RenderScene::refreshPass(MeshPass* pass)
{	
	pass->needsIndirectRefresh = true;
	pass->needsInstanceRefresh = true;

	/*
	*  Delete objects that are already batched.
	*/
	if (pass->objectsToDelete.size() > 0)
	{
		//Create the flat batches that contains the objects to be deleted, so that we can do the deletion on the flatBatches array directly
		std::vector<RenderScene::FlatBatch> deletionBatches;

		deletionBatches.reserve(pass->objectsToDelete.size());

		for (auto object : pass->objectsToDelete) {
			pass->reusableObjects.push_back(object);
			RenderScene::FlatBatch newBatch;

			RenderScene::PassObject passObject = pass->objects[object.handle];
			newBatch.object = object;

			uint64_t pipelinehash = std::hash<uint64_t>()(uint64_t(passObject.material.lock()->pipeline));
			uint64_t sethash = std::hash<uint64_t>()((uint64_t)passObject.material.lock() ->materialSet);

			uint32_t mathash = static_cast<uint32_t>(pipelinehash ^ sethash);

			uint32_t meshmat = uint64_t(mathash) ^ uint64_t(passObject.meshID.handle);

			//pack mesh id and material into 64 bits				
			newBatch.sortKey = uint64_t(meshmat) | (uint64_t(passObject.customKey) << 32);

			pass->objects[object.handle].customKey = 0;
			pass->objects[object.handle].material.reset();
			pass->objects[object.handle].meshID.handle = -1;
			pass->objects[object.handle].original.handle = -1;

			deletionBatches.push_back(newBatch);
		}

		pass->objectsToDelete.clear();

		// Sort the deletion batches based on the sort key.
		{
			std::sort(deletionBatches.begin(), deletionBatches.end(), [](const RenderScene::FlatBatch& A, const RenderScene::FlatBatch& B) {
				if (A.sortKey < B.sortKey) { return true; }
				else if (A.sortKey == B.sortKey) { return A.object.handle < B.object.handle; }
				else { return false; }
				});
		}

		// Do deletion, now the flatBatches array doesn't contain the deleted objects anymore.
		{
			std::vector<RenderScene::FlatBatch> newbatches;
			newbatches.reserve(pass->flatBatches.size());
			{
				std::set_difference(pass->flatBatches.begin(), pass->flatBatches.end(), deletionBatches.begin(), deletionBatches.end(), std::back_inserter(newbatches), [](const RenderScene::FlatBatch& A, const RenderScene::FlatBatch& B) {
					if (A.sortKey < B.sortKey) { return true; }
					else if (A.sortKey == B.sortKey) { return A.object.handle < B.object.handle; }
					else { return false; }
					});
			}
			pass->flatBatches = std::move(newbatches);
		}
	}

	/* 
	*  Create object list of the pass.
	*/
	std::vector<uint32_t> newObjects;
	{
		newObjects.reserve(pass->unbatchedObjects.size());
		for (auto object : pass->unbatchedObjects)
		{
			RenderScene::PassObject newObject;

			newObject.original = object;
			RenderObject* renderObject = getRenderObject(object);
			newObject.meshID = renderObject->meshID;

			//pack mesh id and material into 32 bits
			newObject.material = renderObject->material;

			uint32_t handle = -1;

			//reuse handle
			if (pass->reusableObjects.size() > 0)
			{
				handle = pass->reusableObjects.back().handle;
				pass->reusableObjects.pop_back();
				pass->objects[handle] = newObject;
			}
			else
			{
				handle = pass->objects.size();
				pass->objects.push_back(newObject);
			}

			newObjects.push_back(handle);
			getRenderObject(object)->passIndices[pass->passType] = static_cast<int32_t>(handle);
		}

		pass->unbatchedObjects.clear();
		pass->unbatchedObjects.shrink_to_fit();
	}

	/*
	*  Create the flat draw batch list based on the object list create above.
	*/
	std::vector<RenderScene::FlatBatch> newBatches;
	newBatches.reserve(newObjects.size());
	{
		for (auto object : newObjects) {
			{
				RenderScene::FlatBatch newBatch;

				PassObject passObject = pass->objects[object];
				newBatch.object.handle = object;

				uint64_t pipelinehash = std::hash<uint64_t>()(uint64_t(passObject.material.lock()->pipeline));
				uint64_t sethash = std::hash<uint64_t>()((uint64_t)passObject.material.lock()->materialSet);

				uint32_t mathash = static_cast<uint32_t>(pipelinehash ^ sethash);

				uint32_t meshmat = uint64_t(mathash) ^ uint64_t(passObject.meshID.handle);

				//pack mesh id and material into 64 bits				
				newBatch.sortKey = uint64_t(meshmat) | (uint64_t(passObject.customKey) << 32);

				newBatches.push_back(newBatch);
			}
		}

		// Sort the new batches.
		std::sort(newBatches.begin(), newBatches.end(), [](const RenderScene::FlatBatch& A, const RenderScene::FlatBatch& B) {
			if (A.sortKey < B.sortKey) { return true; }
			else if (A.sortKey == B.sortKey) { return A.object.handle < B.object.handle; }
			else { return false; }
			});
	}
	
	/*
	*   Merge the new batch list into the flatBatches array, and sort the flatBatches array.
	*/
	{
		if (pass->flatBatches.size() > 0 && newBatches.size() > 0)
		{
			size_t index = pass->flatBatches.size();
			pass->flatBatches.reserve(pass->flatBatches.size() + newBatches.size());

			for (auto batch : newBatches)
			{
				pass->flatBatches.push_back(batch);
			}

			RenderScene::FlatBatch* begin = pass->flatBatches.data();
			RenderScene::FlatBatch* mid = begin + index;
			RenderScene::FlatBatch* end = begin + pass->flatBatches.size();
			//std::sort(pass->flatBatches.begin(), pass->flatBatches.end(), [](const RenderScene::FlatBatch& A, const RenderScene::FlatBatch& B) {
			//	return A.sortKey < B.sortKey;
			//	});
			std::inplace_merge(begin, mid, end, [](const RenderScene::FlatBatch& A, const RenderScene::FlatBatch& B) {
				if (A.sortKey < B.sortKey) { return true; }
				else if (A.sortKey == B.sortKey) { return A.object.handle < B.object.handle; }
				else { return false; }
				});
		}
		else if (pass->flatBatches.size() == 0)
		{
			pass->flatBatches = std::move(newBatches);
		}
	}

	/*
	*  Merge flat batches into indirect batches.
	*/
	{
		pass->indirectBatches.clear();

		if (pass->flatBatches.size() > 0)
		{
			RenderScene::IndirectBatch newBatch;
			newBatch.first = 0;
			newBatch.count = 0;
			newBatch.material = pass->get(pass->flatBatches[0].object)->material;
			newBatch.meshID = pass->get(pass->flatBatches[0].object)->meshID;
			pass->indirectBatches.push_back(newBatch);

			RenderScene::IndirectBatch* lastBatch = &pass->indirectBatches.back();
			MaterialInstance* lastMaterial = lastBatch->getMaterial();
			for(int i = 0; i < pass->flatBatches.size(); i++)
			{
				PassObject* passObject = pass->get(pass->flatBatches[i].object);
				bool isSameMaterial = false;
				bool isSameMesh = passObject->meshID.handle == lastBatch->meshID.handle;

				if (*passObject->getMaterial() == *lastMaterial)
				{
					isSameMaterial = true;
				}

				if (!isSameMaterial)
				{
					newBatch.material = passObject->material;

					if (newBatch.material.lock() == lastBatch->material.lock())
					{
						isSameMaterial = true;
					}
				}

				if (isSameMaterial && isSameMesh)
				{
					lastBatch->count++;
				}
				else
				{
					newBatch.first = i;
					newBatch.count = 1;
					newBatch.meshID = passObject->meshID;

					pass->indirectBatches.push_back(newBatch);
					lastBatch = &pass->indirectBatches.back();
				}
			}
		}
	}

	/*
	*  Merge indirect batches into multi batches.
	*/
	{
		pass->multiBatches.clear();

		MultiBatch newBatch;
		newBatch.count = 1;
		newBatch.first = 0;

		for(int i = 0; i < pass->indirectBatches.size(); i++)
		{
			IndirectBatch* joinBatch = &pass->indirectBatches[newBatch.first];
			IndirectBatch* nextBatch = &pass->indirectBatches[i];

			bool isMeshCompatible = getMesh(joinBatch->meshID)->isMerged;
			bool isSameMat = false;

			if(*joinBatch->getMaterial() == *nextBatch->getMaterial())
			{
				isSameMat = true;
			}

			if(!isSameMat || !isMeshCompatible)
			{
				pass->multiBatches.push_back(newBatch);
				newBatch.count = 1;
				newBatch.first = i;
			}
			else
			{
				newBatch.count++;
			}
		}
		pass->multiBatches.push_back(newBatch);
	}
}

void RenderScene::clearDirtyObjects()
{
	for (auto obj : dirtyRenderObjects)
	{
		getRenderObject(obj)->updateIndex = (uint32_t)-1;
	}
	dirtyRenderObjects.clear();
	dirtyRenderObjects.shrink_to_fit();
}

DrawMesh* RenderScene::getMesh(Handle<DrawMesh> meshID)
{
	return &drawMeshes[meshID.handle];
}

RenderScene::PassObject* RenderScene::MeshPass::get(Handle<PassObject> handle)
{
	return &objects[handle.handle];
}