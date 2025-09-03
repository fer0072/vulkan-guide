#include <vk_scene.h>
#include <vk_engine.h>

void VulkanEngine::prepareMeshData(VkCommandBuffer cmd)
{

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