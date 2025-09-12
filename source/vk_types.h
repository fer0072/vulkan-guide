// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
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

namespace vkGlobals
{
    extern float g_zNear;
    extern float g_zFar;
    extern float g_maxDrawDist;
}

class VulkanEngine;

// we will add our main reusable types here
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;

    static AllocatedImage createImage(VulkanEngine* engine, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    static AllocatedImage createImage(VulkanEngine* engine, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    static void destroyImage(VkDevice device, VmaAllocator allocator, const AllocatedImage& img);
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
	size_t size = 0;

    static AllocatedBuffer createBuffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    static void* mapBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer);
    static void unmapBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer);
    static void destroyBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer);
};

struct GPUGLTFMaterial {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    glm::vec4 extra[14];
};

static_assert(sizeof(GPUGLTFMaterial) == 256);

struct LightData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct SceneData {
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

    bool operator==(const MaterialInstance& other) const
    {
        return pipeline == other.pipeline && materialSet == other.materialSet && passType == other.passType;
    }
};

//< mat_types
//> vbuf_types
struct Vertex {
	glm::vec4 position_uvx;
	glm::vec4 normal_uvy;
	glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};
//< vbuf_types

struct DirectionalLight
{
    glm::vec3 lightPosition;
    glm::vec3 lightDirection;
    glm::vec3 shadowExtent;

    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();
};

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