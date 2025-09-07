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

    PerPassData<int32_t> passIndices;

    Bounds bounds;
    glm::mat4 transform;
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
        Handle<DrawMesh> meshID;
        std::weak_ptr<MaterialInstance> material;
        uint32_t customKey;
        uint32_t objectDataIndex;

        MaterialInstance* getMaterial()
        {
            return material.lock().get();
        }
    };

    // Batch that is not merged yet.
    struct FlatBatch 
    {
        Handle<PassObject> object;
        uint64_t sortKey;

        bool operator==(const FlatBatch& other) const
        {
            return object.handle == other.object.handle && sortKey == other.sortKey;
        }
    };

    struct IndirectBatch {
        Handle<DrawMesh> meshID;
        std::weak_ptr <MaterialInstance> material;
        uint32_t first;
        uint32_t count;

        MaterialInstance* getMaterial()
        {
            return material.lock().get();
		}
    };

    struct MultiBatch {
        uint32_t first;
        uint32_t count;
    };

    struct MeshPass 
    {
        std::vector<RenderScene::MultiBatch> multiBatches;
        std::vector<RenderScene::IndirectBatch> indirectBatches;
        std::vector<Handle<RenderObject>> unbatchedObjects;
        std::vector<RenderScene::FlatBatch> flatBatches;
        std::vector<PassObject> objects;
        std::vector<Handle<PassObject>> reusableObjects;
        std::vector<Handle<PassObject>> objectsToDelete;

        std::optional<AllocatedBuffer> compactedInstanceBuffer;
        std::optional<AllocatedBuffer> GPUInstanceBuffer;

        std::optional<AllocatedBuffer> drawIndirectBuffer;
        std::optional<AllocatedBuffer> completeIndirectBuffer;

        PassObject* get(Handle<PassObject> handle);

        MaterialPass passType;

        bool needsIndirectRefresh = false;
        bool needsInstanceRefresh = false;
    };

    std::vector<RenderObject> allRenderObjects;

    MeshPass shadowPass;
    MeshPass forwardOpaquePass;
    MeshPass forwardTransparentPass;

    // Merged vertex buffer, index buffer and object data buffer that contains data of all render objects.
    std::optional<AllocatedBuffer> mergedVertexBuffer;
    std::optional<AllocatedBuffer> mergedIndexBuffer;
    std::optional<AllocatedBuffer> objectDataBuffer;

public:

    void prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine);

    void prepareMeshData(VkCommandBuffer cmd, VulkanEngine* engine);

    RenderObject* getRenderObject(Handle<RenderObject> objectID);

    Handle<DrawMesh> getMeshHandle(GeoSurface* surface, std::shared_ptr<MeshAsset> meshAsset);

    DrawMesh* getMesh(Handle<DrawMesh> meshID);

    void mergeMeshes(VulkanEngine* engine);

    void buildBatches();

private:

    // MeshAssets and its start index in the merged buffers.
	std::map<MeshAsset*, std::pair<uint32_t, uint32_t> /*index in the merged vertex buffer and index buffer*/> cachedMeshAssets; 

    std::vector<DrawMesh> drawMeshes;

    std::vector<VkBufferMemoryBarrier> cullReadyBarriers;

private:

    void buildPassBatches(MeshPass* pass);

    void prepareComputeCullData(VkCommandBuffer cmd, VulkanEngine* engine, MeshPass& meshPass);
};