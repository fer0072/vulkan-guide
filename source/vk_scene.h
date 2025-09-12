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
};

struct InstanceID {
    uint32_t objectID;
    uint32_t batchID;
};

struct DrawMesh
{
    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t firstVertex;
    bool isMerged = false;

    std::shared_ptr<MeshAsset> meshAsset;
};

struct RenderObject 
{
    Handle<DrawMesh> meshID;
    std::shared_ptr<MaterialInstance> material;

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
        MaterialPass passType;

        PassObject* get(Handle<PassObject> handle);

        /*
		*  Batches of this pass.
        */
        std::vector<RenderScene::FlatBatch> flatBatches;
        std::vector<RenderScene::InstanceBatch> instanceBatches;
        std::vector<RenderScene::IndirectBatch> indirectBatches;

        /*
		*  Buffers that used as input / output of the actual rendering passes.
        */
		// Input of compute cull pass. Object ID and instance batch ID of each instance.
        std::optional<AllocatedBuffer> instanceIDBuffer;
        // Output of compute cull pass and input of forward pass. Output of compute cull pass. Object ID of each instance, only visible instance has value larger than 0.
        std::optional<AllocatedBuffer> culledInstanceObjectIDBuffer;

		// Input of compute cull pass. All information of each VkDrawIndexedIndirectCommand, but instanceCount is 0.
        std::optional<AllocatedBuffer> initialDrawIndirectBuffer;
        // Output of compute cull pass and input of forward pass. Final drawIndirectBuffer that is used in the actual vkCmdDrawIndexedIndirect, instanceCount is updated by compute cull pass.
        std::optional<AllocatedBuffer> drawIndirectBuffer;
    };

    MeshPass shadowPass;
    MeshPass forwardOpaquePass;
    MeshPass forwardTransparentPass;

    /*
    *  Buffers shared by all MeshPasses.
    */ 
    std::vector<RenderObject> allRenderObjects;
    // Merged vertex buffer, index buffer and object data buffer that contains data of all render objects.
    std::optional<AllocatedBuffer> mergedVertexBuffer;
    std::optional<AllocatedBuffer> mergedIndexBuffer;
    std::optional<AllocatedBuffer> objectDataBuffer;    

public:

    void createPassBuffers(VulkanEngine* engine);

    void preparePassData(VkCommandBuffer cmd, VulkanEngine* engine);

    void mergeMeshes(VulkanEngine* engine);

    void buildBatches();

    Handle<DrawMesh> generateDrawMesh(GeoSurface* surface, std::shared_ptr<MeshAsset> meshAsset);

private:

    // MeshAssets and its start index in the merged buffers.
	std::map<std::shared_ptr<MeshAsset>, std::pair<uint32_t, uint32_t> /*index in the merged vertex buffer and index buffer*/> cachedMeshAssets;

    std::vector<DrawMesh> drawMeshes;

    std::vector<VkBufferMemoryBarrier> cullReadyBarriers;
    std::vector<VkBufferMemoryBarrier> uploadBarriers;

    MeshPass* meshPasses[3] = { &shadowPass, &forwardOpaquePass, &forwardTransparentPass };

private:

    void buildPassBatches(MeshPass* pass);

    RenderObject* getRenderObject(Handle<RenderObject> objectID);

    DrawMesh* getMesh(Handle<DrawMesh> meshID);

	void uploadObjectData(VkCommandBuffer cmd, VulkanEngine* engine);

    void uploadPassData(VkCommandBuffer cmd, VulkanEngine* engine, MeshPass* meshPass);
};