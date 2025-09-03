#include <vk_scene.h>
#include <vk_engine.h>

void VulkanEngine::prepareMeshData(VkCommandBuffer cmd)
{

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

		forwardTransparentPass.unbatchedObjects.push_back(objectID);
		forwardTransparentPass.objectsToDelete.push_back(obj);

		passIndices[MaterialPass::forwardTransparent] = -1;
	}


	if (getRenderObject(objectID)->updateIndex == (uint32_t)-1)
	{
		getRenderObject(objectID)->updateIndex = static_cast<uint32_t>(dirtyObjects.size());

		dirtyObjects.push_back(objectID);
	}
}

RenderObject* RenderScene::getRenderObject(Handle<RenderObject> objectID)
{
	return &renderables[objectID.handle];
}

Handle<DrawMesh> RenderScene::getMeshHandle(const GeoSurface& surface, std::shared_ptr<OriginalMesh> originalMesh)
{
	Handle<DrawMesh> handle;
	auto it = convertedMesh.find(originalMesh.get());
	if (it == convertedMesh.end())
	{
		uint32_t index = static_cast<uint32_t>(drawMeshes.size());

		DrawMesh newMesh;
		newMesh.original = originalMesh;
		newMesh.firstIndex = surface.startIndex;
		newMesh.firstVertex = surface.startVertex;
		newMesh.vertexCount = static_cast<uint32_t>(originalMesh->_vertices.size());
		newMesh.indexCount = static_cast<uint32_t>(originalMesh->_indices.size());

		drawMeshes.push_back(newMesh);

		handle.handle = index;
		convertedMesh[originalMesh.get()] = handle;
	}
	else {
		handle = (*it).second;
	}
	return handle;
}

void RenderScene::buildBatches()
{
	//refreshPass(&shadowPass);
	refreshPass(&forwardOpaquePass);
	refreshPass(&forwardTransparentPass);
}

void RenderScene::refreshPass(MeshPass* pass)
{	
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
}

RenderScene::PassObject* RenderScene::MeshPass::get(Handle<PassObject> handle)
{
	return &objects[handle.handle];
}