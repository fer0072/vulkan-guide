#include <vk_scene.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include "vk_mem_alloc.h"
#include <future>

void RenderScene::prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine)
{
	cullReadyBarriers.clear();

	//prepareComputeCullData(cmd, engine, _renderScene.shadowPass);
	prepareComputeCullData(cmd, engine, forwardOpaquePass);
	//prepareComputeCullData(cmd, engine, forwardTransparentPass);

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, (uint32_t)cullReadyBarriers.size(), cullReadyBarriers.data(), 0, nullptr);
}

void RenderScene::prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine, MeshPass& meshPass)
{
	// Copy the complete indirect buffer into the one we actually use during rendering. This happens every frame.
	VkBufferCopy indirectCopy;
	indirectCopy.dstOffset = 0;
	indirectCopy.size = meshPass.instanceBatches.size() * sizeof(VkDrawIndexedIndirectCommand);
	indirectCopy.srcOffset = 0;
	vkCmdCopyBuffer(cmd, meshPass.initialDrawIndirectBuffer.value().buffer, meshPass.drawIndirectBuffer.value().buffer, 1, &indirectCopy);

	//TBD, create compute queue family?
	VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(meshPass.drawIndirectBuffer.value().buffer, engine->getGraphicsQueueFamily(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

	cullReadyBarriers.push_back(barrier);
}


void RenderScene::prepareMeshData(VkCommandBuffer cmd, VulkanEngine* engine)
{
	std::vector<VkBufferMemoryBarrier> uploadBarriers;

	/*
	*   Upload object data to GPU.
	*/
	{
		size_t copySize = allRenderObjects.size() * sizeof(GPUObjectData);
		if (!objectDataBuffer.has_value() || objectDataBuffer.value().size < copySize)
		{
			objectDataBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(),copySize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			engine->_mainDeletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), objectDataBuffer.value());
				});
		}

		AllocatedBuffer newBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), copySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		GPUObjectData* objectSSBO = (GPUObjectData*)AllocatedBuffer::mapBuffer(engine->getAllocator(), newBuffer);
		for (int i = 0; i < allRenderObjects.size(); i++)
		{
			Handle<RenderObject> objHandle;
			objHandle.handle = i;
			RenderObject* obj = getRenderObject(objHandle);

			GPUObjectData objectData;
			objectData.transform = obj->transform;
			objectData.boundOriginAndRadius = glm::vec4(obj->bounds.origin, obj->bounds.sphereRadius);

			memcpy(objectSSBO + i, &objectData, sizeof(GPUObjectData));
		}
		AllocatedBuffer::unmapBuffer(engine->getAllocator(), newBuffer);

		//copy from the uploaded cpu side instance buffer to the gpu one
		VkBufferCopy bufferCopy;
		bufferCopy.srcOffset = 0;
		bufferCopy.dstOffset = 0;
		bufferCopy.size = allRenderObjects.size() * sizeof(GPUObjectData);
		vkCmdCopyBuffer(cmd, newBuffer.buffer, objectDataBuffer.value().buffer, 1, &bufferCopy);

		VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(objectDataBuffer.value().buffer, engine->getGraphicsQueueFamily(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

		engine->getCurrentFrame()._deletionQueue.push_function([=]() {
			AllocatedBuffer::destroyBuffer(engine->getAllocator(), newBuffer);
			});

		uploadBarriers.emplace_back(barrier);
	}

	/*
	*  Create indirect buffer on GPU.
	*/
	//TBD MeshPass* passes[3] = { &shadowPass, &forwardOpaquePass, &forwardTransparentPass };
	MeshPass* passes[2] = { &forwardOpaquePass, &forwardTransparentPass };
	for (int i = 0; i < 2; i++)
	{
		MeshPass* pass = passes[i];

		if (!pass->drawIndirectBuffer.has_value() || pass->drawIndirectBuffer.value().size < pass->instanceBatches.size() * sizeof(VkDrawIndexedIndirectCommand))
		{
			pass->drawIndirectBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), pass->instanceBatches.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), pass->drawIndirectBuffer.value());
				});
		}

		if (!pass->culledInstanceObjectIDBuffer.has_value() || pass->culledInstanceObjectIDBuffer.value().size < pass->flatBatches.size() * sizeof(uint32_t))
		{
			pass->culledInstanceObjectIDBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), pass->flatBatches.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), pass->culledInstanceObjectIDBuffer.value());
				});
		}

		if (!pass->instanceIDBuffer.has_value() || pass->instanceIDBuffer.value().size < pass->flatBatches.size() * sizeof(InstanceID))
		{
			pass->instanceIDBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), pass->flatBatches.size() * sizeof(InstanceID), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			engine->_mainDeletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), pass->instanceIDBuffer.value());
				});
		}
	}

	/*
	*   Upload data to buffers above.
	*/
	for (int i = 0; i < 2; i++)
	{
		MeshPass* pass = passes[i];

		if (pass->needsInitialDrawIndirectBufferRefresh && pass->instanceBatches.size() > 0)
		{
			AllocatedBuffer newInitialDrawIndirectBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), pass->instanceBatches.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			VkDrawIndexedIndirectCommand* IndirectCommandData = (VkDrawIndexedIndirectCommand*)AllocatedBuffer::mapBuffer(engine->getAllocator(), newInitialDrawIndirectBuffer);
			for (int i = 0; i < pass->instanceBatches.size(); i++) {

				InstanceBatch instanceBatch = pass->instanceBatches[i];

				IndirectCommandData[i].firstInstance = instanceBatch.firstInstance;
				IndirectCommandData[i].instanceCount = 0;
				IndirectCommandData[i].firstIndex = getMesh(instanceBatch.meshID)->firstIndex;
				IndirectCommandData[i].vertexOffset = getMesh(instanceBatch.meshID)->firstVertex;
				IndirectCommandData[i].indexCount = getMesh(instanceBatch.meshID)->indexCount;
			}
			AllocatedBuffer::unmapBuffer(engine->getAllocator(), newInitialDrawIndirectBuffer);
			engine->getCurrentFrame()._deletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), newInitialDrawIndirectBuffer);
				});
			
			pass->initialDrawIndirectBuffer = std::move(newInitialDrawIndirectBuffer);
			pass->needsInitialDrawIndirectBufferRefresh = false;
		}

		if (pass->needsInstanceIDBufferRefresh && pass->flatBatches.size() > 0)
		{
			AllocatedBuffer newInstanceIDBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), pass->flatBatches.size() * sizeof(InstanceID), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			InstanceID* instanceData = (InstanceID*)AllocatedBuffer::mapBuffer(engine->getAllocator(), newInstanceIDBuffer);

			uint32_t dataIndex = 0;
			for (uint32_t i = 0; i < pass->instanceBatches.size(); i++) {
				InstanceBatch instanceBatch = pass->instanceBatches[i];

				for (uint32_t flatBatchIndex = 0; flatBatchIndex < instanceBatch.instanceCount; flatBatchIndex++)
				{
					instanceData[dataIndex].objectID = pass->get(pass->flatBatches[instanceBatch.firstInstance + flatBatchIndex].object)->objectID.handle;
					instanceData[dataIndex].batchID = i;
					dataIndex++;
				}
			}

			AllocatedBuffer::unmapBuffer(engine->getAllocator(), newInstanceIDBuffer);
			engine->getCurrentFrame()._deletionQueue.push_function([=]() {
				AllocatedBuffer::destroyBuffer(engine->getAllocator(), newInstanceIDBuffer);
				});

			VkBufferCopy instanceCopy;
			instanceCopy.srcOffset = 0;
			instanceCopy.dstOffset = 0;
			instanceCopy.size = pass->flatBatches.size() * sizeof(InstanceID);
			vkCmdCopyBuffer(cmd, newInstanceIDBuffer.buffer, pass->instanceIDBuffer.value().buffer, 1, &instanceCopy);

			pass->needsInstanceIDBufferRefresh = false;

			VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(pass->instanceIDBuffer.value().buffer, engine->getGraphicsQueueFamily(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

			uploadBarriers.push_back(barrier);
		}
	}

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(uploadBarriers.size()), uploadBarriers.data(), 0, nullptr);

	uploadBarriers.clear();
}

RenderObject* RenderScene::getRenderObject(Handle<RenderObject> objectID)
{
	return &allRenderObjects[objectID.handle];
}

Handle<DrawMesh> RenderScene::generateDrawMesh(GeoSurface* surface, std::shared_ptr<MeshAsset> meshAsset)
{
	auto it = cachedMeshAssets.find(meshAsset);
	if (it == cachedMeshAssets.end())
	{
		cachedMeshAssets[meshAsset] = std::make_pair(0, 0);
	}

	DrawMesh newMesh;
	newMesh.meshAsset = meshAsset;
	newMesh.indexCount = surface->indicesCount;
	newMesh.firstIndex = surface->startIndex;
	newMesh.firstVertex = 0;

	Handle<DrawMesh> handle;
	uint32_t index = static_cast<uint32_t>(drawMeshes.size());
	drawMeshes.push_back(newMesh);
	handle.handle = index;
	return handle;
}

void RenderScene::mergeMeshes(VulkanEngine* engine)
{
	uint32_t totalVertices = 0;
	uint32_t totalIndices = 0;

	for(auto& cache: cachedMeshAssets)
	{
		std::shared_ptr<MeshAsset> meshAsset = cache.first;
		std::pair<uint32_t, uint32_t>& startIndices = cache.second;

		startIndices.first = totalVertices;
		startIndices.second = totalIndices;

		totalVertices += (uint32_t)meshAsset->meshBuffers.vertexBuffer.size;
		totalIndices += (uint32_t)meshAsset->meshBuffers.indexBuffer.size;
	}

	for (auto& drawMesh : drawMeshes)
	{
		std::shared_ptr<MeshAsset> meshAsset = drawMesh.meshAsset;
		std::pair<uint32_t, uint32_t> startIndices = cachedMeshAssets[meshAsset];
		drawMesh.firstVertex = startIndices.first;
		drawMesh.firstIndex += startIndices.second;
		drawMesh.isMerged = true;
	}

	mergedVertexBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), totalVertices * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	mergedIndexBuffer = AllocatedBuffer::createBuffer(engine->getAllocator(), totalIndices * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	engine->immediateSubmit([&](VkCommandBuffer cmd) {
		for (auto& cache : cachedMeshAssets)
		{
			std::shared_ptr<MeshAsset> meshAsset = cache.first;
			std::pair<uint32_t, uint32_t> startIndices = cache.second;

			VkBufferCopy vertexCopy;
			vertexCopy.dstOffset = startIndices.first * sizeof(Vertex);
			vertexCopy.size = meshAsset->meshBuffers.vertexBuffer.size;
			vertexCopy.srcOffset = 0;

			vkCmdCopyBuffer(cmd, meshAsset->meshBuffers.vertexBuffer.buffer, mergedVertexBuffer->buffer, 1, &vertexCopy);

			VkBufferCopy indexCopy;
			indexCopy.dstOffset = startIndices.second * sizeof(uint32_t);
			indexCopy.size = meshAsset->meshBuffers.indexBuffer.size;
			indexCopy.srcOffset = 0;

			vkCmdCopyBuffer(cmd, meshAsset->meshBuffers.indexBuffer.buffer, mergedIndexBuffer->buffer, 1, &indexCopy);
		}
		});

	engine->_mainDeletionQueue.push_function([=]() {
		AllocatedBuffer::destroyBuffer(engine->getAllocator(), mergedVertexBuffer.value());
		AllocatedBuffer::destroyBuffer(engine->getAllocator(), mergedIndexBuffer.value());
		});
}

void RenderScene::buildBatches()
{
	//buildPassBatches(&shadowPass);
	buildPassBatches(&forwardOpaquePass);
	buildPassBatches(&forwardTransparentPass);
}

void RenderScene::buildPassBatches(MeshPass* pass)
{	
	pass->needsInitialDrawIndirectBufferRefresh = true;
	pass->needsInstanceIDBufferRefresh = true;

	/* 
	*  Create object list of the pass.
	*/
	std::vector<uint32_t> newObjects;
	{
		newObjects.reserve(pass->unbatchedObjects.size());
		for (auto object : pass->unbatchedObjects)
		{
			RenderScene::PassObject newObject;
			newObject.objectID = object;

			uint32_t handle = (uint32_t)pass->objects.size();
			pass->objects.push_back(newObject);
			newObjects.push_back(handle);
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
				PassObject passObject = pass->objects[object];
				RenderObject* renderObject = getRenderObject(passObject.objectID);
				std::shared_ptr <MaterialInstance> material = renderObject->material;

				uint64_t pipelineHash = std::hash<uint64_t>()(uint64_t(material->pipeline->pipeline));
				uint64_t setHash = std::hash<uint64_t>()((uint64_t)material->materialSet);

				uint32_t mathash = static_cast<uint32_t>(pipelineHash | setHash);

				uint32_t meshmat = uint64_t(mathash) ^ uint64_t(renderObject->meshID.handle);

				RenderScene::FlatBatch newBatch;
				newBatch.object.handle = object;
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
	*  Merge flat batches into instance batches.
	*/
	{
		pass->instanceBatches.clear();
		
		if (pass->flatBatches.size() > 0)
		{
			PassObject* firstObject = pass->get(pass->flatBatches[0].object);
			RenderObject* firstRenderObject = getRenderObject(firstObject->objectID);
			RenderScene::InstanceBatch newBatch;
			newBatch.firstInstance = 0;
			newBatch.instanceCount = 1;
			newBatch.material = firstRenderObject->material;
			newBatch.meshID = firstRenderObject->meshID;
			pass->instanceBatches.push_back(newBatch);

			for(int i = 1; i < pass->flatBatches.size(); i++)
			{
				PassObject* passObject = pass->get(pass->flatBatches[i].object);
				RenderObject* renderObject = getRenderObject(passObject->objectID);
				RenderScene::InstanceBatch& lastInstanceBatch = pass->instanceBatches.back();

				bool isSameMaterial = *renderObject->material.get() == *lastInstanceBatch.getMaterial();
				bool isSameMesh = getMesh(renderObject->meshID)->meshAsset == getMesh(lastInstanceBatch.meshID)->meshAsset;

				if (isSameMaterial && isSameMesh)
				{
					lastInstanceBatch.instanceCount++;
				}
				else
				{
					newBatch.firstInstance = i;
					newBatch.instanceCount = 1;
					newBatch.material = renderObject->material;
					newBatch.meshID = renderObject->meshID;
					pass->instanceBatches.push_back(newBatch);
				}
			}
		}
	}

	/*
	*  Merge instance batches into indirect batches.
	*/
	{
		pass->indirectBatches.clear();

		IndirectBatch newBatch;
		newBatch.count = 1;
		newBatch.firstInstanceBatch = 0;
		pass->indirectBatches.push_back(newBatch);

		for(int i = 1; i < pass->instanceBatches.size(); i++)
		{
			IndirectBatch& lastIndirectBatch = pass->indirectBatches.back();
			InstanceBatch lastInstanceBatch = pass->instanceBatches[lastIndirectBatch.firstInstanceBatch];
			InstanceBatch nextInstanceBatch = pass->instanceBatches[i];

			bool isMeshCompatible = getMesh(lastInstanceBatch.meshID)->isMerged && getMesh(nextInstanceBatch.meshID)->isMerged;
			bool isSameMat = *lastInstanceBatch.getMaterial() == *nextInstanceBatch.getMaterial();

			if (isSameMat && isMeshCompatible)
			{
				lastIndirectBatch.count++;
			}
			else
			{
				newBatch.count = 1;
				newBatch.firstInstanceBatch = i;
				pass->indirectBatches.push_back(newBatch);
			}
		}
	}
}

DrawMesh* RenderScene::getMesh(Handle<DrawMesh> meshID)
{
	return &drawMeshes[meshID.handle];
}

RenderScene::PassObject* RenderScene::MeshPass::get(Handle<PassObject> handle)
{
	return &objects[handle.handle];
}