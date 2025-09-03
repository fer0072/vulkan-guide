// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
//> intro
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
//< intro 

// we will add our main reusable types here
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct GPUGLTFMaterial {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    glm::vec4 extra[14];
};

static_assert(sizeof(GPUGLTFMaterial) == 256);

struct GPU_sceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

//> mat_types
enum class MaterialPass :uint8_t {
    shadow = 0,
    forwardOpaque = 1,
    forwardTransparent = 2,
    other
};

template<typename T>
struct PerPassData
{
public:
    T& operator[](MaterialPass pass)
    {
        switch (pass)
        {
        case MaterialPass::shadow:
            return data[0];
        case MaterialPass::forwardOpaque:
            return data[1];
        case MaterialPass::forwardTransparent:
            return data[2];
        }
        assert(false);
        return data[0];
    };

    void clear(T&& val)
    {
        for (int i = 0; i < 3; i++)
        {
            data[i] = val;
        }
    }

private:
    std::array<T, 3> data;
};

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};
//< mat_types
//> vbuf_types
struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct OriginalMesh
{
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    //TBD
    VkDeviceAddress vertexBufferAddress;

    std::shared_ptr<OriginalMesh> original;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};
//< vbuf_types

//> node_types
class RenderScene;

// base class for a renderable dynamic object
class IRenderable {
    virtual void generateRenderObject(const glm::mat4& topMatrix, RenderScene& scene) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void generateRenderObject(const glm::mat4& topMatrix, RenderScene& scene)
    {
        // Iterate all children nodes, generate render objects.
        for (auto& c : children) {
            c->generateRenderObject(topMatrix, scene);
        }
    }
};
//< node_types
//> intro
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
             fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
//< intro