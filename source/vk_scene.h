#pragma once

#include <vk_types.h>
#include <vk_loader.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <vector>
#include <map>

template<typename T>
struct Handle 
{
    uint32_t handle;
};

struct GPUObjectData
{
    glm::mat4 transform;
    glm::vec4 boundOriginAndRadius;
    glm::vec4 boundExtent;
};

struct GPUIndirectObject {
    VkDrawIndexedIndirectCommand command;
    uint32_t objectID;
    uint32_t batchID;
};

struct GPUInstance {
    uint32_t objectID;
    uint32_t batchID;
};

struct DrawMesh
{
    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t firstVertex;
    bool isMerged = false;

    std::weak_ptr<MeshAsset> meshAsset;

    MeshAsset* getMeshAsset()
    {
        return meshAsset.lock().get();
	}
};

struct RenderObject 
{
    Handle<DrawMesh> meshID;
    std::weak_ptr<MaterialInstance> material;

    Bounds bounds;
    glm::mat4 transform;

    MaterialInstance* getMaterial()
    {
        return material.lock().get();
    }
};

class RenderScene
{
public:
    RenderScene() {
        shadowPass.passType = MaterialPass::shadow;
        forwardOpaquePass.passType = MaterialPass::forwardOpaque;
        forwardTransparentPass.passType = MaterialPass::forwardTransparent;
    }

    struct PassObject 
    {
        Handle<RenderObject> objectID;
        uint32_t customKey;
    };

	// Batch that is not merged yet, can use to generate regular draw calls.
    struct FlatBatch 
    {
        Handle<PassObject> object;
        uint64_t sortKey;

        bool operator==(const FlatBatch& other) const
        {
            return object.handle == other.object.handle && sortKey == other.sortKey;
        }
    };

    // Can use to generate instanced draw calls.
    struct InstanceBatch {
        Handle<DrawMesh> meshID;
        std::weak_ptr <MaterialInstance> material;
        uint32_t firstInstance;
        uint32_t instanceCount;

        MaterialInstance* getMaterial()
        {
            return material.lock().get();
		}
    };

    // Can use to generate indirect draw calls.
    struct IndirectBatch {
        uint32_t firstInstanceBatch;
        uint32_t count;
    };

    struct MeshPass 
    {
        std::vector<Handle<RenderObject>> unbatchedObjects;
        std::vector<PassObject> objects;

        std::vector<RenderScene::FlatBatch> flatBatches;
        std::vector<RenderScene::InstanceBatch> instanceBatches;
        std::vector<RenderScene::IndirectBatch> indirectBatches;

        std::optional<AllocatedBuffer> compactedInstanceBuffer;
        std::optional<AllocatedBuffer> GPUInstanceBuffer;

        std::optional<AllocatedBuffer> drawIndirectBuffer;
        std::optional<AllocatedBuffer> completeIndirectCommandBuffer;

        PassObject* get(Handle<PassObject> handle);

        MaterialPass passType;

        bool needsInstanceCommandsBufferRefresh = false;
        bool needsGPUInstanceBufferRefresh = false;
    };

    std::vector<RenderObject> allRenderObjects;
    // Merged vertex buffer, index buffer and object data buffer that contains data of all render objects.
    std::optional<AllocatedBuffer> mergedVertexBuffer;
    std::optional<AllocatedBuffer> mergedIndexBuffer;
    std::optional<AllocatedBuffer> objectDataBuffer;

    MeshPass shadowPass;
    MeshPass forwardOpaquePass;
    MeshPass forwardTransparentPass;

public:

    void prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine);

    void prepareMeshData(VkCommandBuffer cmd, VulkanEngine* engine);

    void mergeMeshes(VulkanEngine* engine);

    void buildBatches();

    Handle<DrawMesh> generateDrawMesh(GeoSurface* surface, std::shared_ptr<MeshAsset> meshAsset);

private:

    // MeshAssets and its start index in the merged buffers.
	std::map<MeshAsset*, std::pair<uint32_t, uint32_t> /*index in the merged vertex buffer and index buffer*/> cachedMeshAssets; 

    std::vector<DrawMesh> drawMeshes;

    std::vector<VkBufferMemoryBarrier> cullReadyBarriers;

private:

    void buildPassBatches(MeshPass* pass);

    void prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine, MeshPass& meshPass);

    RenderObject* getRenderObject(Handle<RenderObject> objectID);

    DrawMesh* getMesh(Handle<DrawMesh> meshID);

};