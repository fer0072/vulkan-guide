#pragma once

#include <vk_types.h>
#include <vk_loader.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <vector>
#include <unordered_map>

template<typename T>
struct Handle 
{
    uint32_t handle;
};

struct DrawMesh
{
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t firstVertex;
    bool isMerged = false;

    std::weak_ptr<OriginalMesh> original;
};

struct RenderObject 
{
    // TBD
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;
    VkDeviceAddress vertexBufferAddress;

    Handle<DrawMesh> meshID;
    std::shared_ptr<MaterialInstance> material;

	// Mark if the object is in the dirty objects list that waited to be updated to gpu.
    uint32_t updateIndex = 0;

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
        std::weak_ptr<MaterialInstance> material;
        Handle<DrawMesh> meshID;
        Handle<RenderObject> original;
        int32_t builtbatch;
        uint32_t customKey;
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

    struct MeshPass 
    {
        /*std::vector<RenderScene::Multibatch> multibatches;
        std::vector<RenderScene::IndirectBatch> batches;*/
        std::vector<Handle<RenderObject>> unbatchedObjects;
        std::vector<RenderScene::FlatBatch> flatBatches;
        std::vector<PassObject> objects;
        std::vector<Handle<PassObject>> reusableObjects;
        std::vector<Handle<PassObject>> objectsToDelete;

        AllocatedBuffer compactedInstanceBuffer;
        AllocatedBuffer passObjectsBuffer;

        AllocatedBuffer drawIndirectBuffer;
        AllocatedBuffer clearIndirectBuffer;

        PassObject* get(Handle<PassObject> handle);

        MaterialPass passType;

        bool needsIndirectRefresh = true;
        bool needsInstanceRefresh = true;
    };

	// Dirty objects is objects whose data has not been updated to the gpu yet.
    std::vector<Handle<RenderObject>> dirtyObjects;
    std::vector<RenderObject> renderables;

    MeshPass shadowPass;
    MeshPass forwardOpaquePass;
    MeshPass forwardTransparentPass;

public:

	void updateObject(Handle<RenderObject> handle);

    RenderObject* getRenderObject(Handle<RenderObject> objectID);

    Handle<DrawMesh> getMeshHandle(const GeoSurface& surface, std::shared_ptr<OriginalMesh> originalMesh);

    void buildBatches();

    void refreshPass(MeshPass* pass);

private:

    std::unordered_map<OriginalMesh*, Handle<DrawMesh>> convertedMesh;

    std::vector<DrawMesh> drawMeshes;

private:

    
};