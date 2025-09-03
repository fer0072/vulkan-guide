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
    Handle<DrawMesh> meshID;
    MaterialInstance* material;

    uint32_t updateIndex = 0;
    uint32_t customSortKey = 0;

    PerPassData<int32_t> passIndices;

    Bounds bounds;
    glm::mat4 transform;
};

class RenderScene
{
public:
    std::vector<Handle<RenderObject>> dirtyObjects;
    std::vector<RenderObject> renderables;

public:

    Handle<DrawMesh> getMeshHandle(const GeoSurface& surface, std::shared_ptr<OriginalMesh> originalMesh);

private:

    std::unordered_map<OriginalMesh*, Handle<DrawMesh>> convertedMesh;

    std::vector<DrawMesh> drawMeshes;

private:

    
};