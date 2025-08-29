// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

#include <deque>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <vk_mem_alloc.h>

#include <camera.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_pipelines.h>
struct MeshAsset;
namespace fastgltf {
struct Mesh;
}

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;
    Bounds bounds;
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct FrameData {
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
    std::vector<RenderObject> transparentSurfaces;
};

struct EngineStats {
    float frameTime;
    int triangleCount;
    int drawcallCount;
    float meshDrawTime;
};

struct GLTFMetallic_Roughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // padding, we need it anyway for uniform buffers
        uint32_t colorTexID;
        uint32_t metalRoughTexID;
        uint32_t pad1;
        uint32_t pad2;
        glm::vec4 extra[13];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void buildPipelines(VulkanEngine* engine);
    void clearResources(VkDevice device);

    MaterialInstance updateMaterialDescriptorSets(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node {

    std::shared_ptr<MeshAsset> mesh;

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};
struct TextureID {
    uint32_t index;
};

struct TextureCache {

    std::vector<VkDescriptorImageInfo> cache;
    TextureID addTexture(const VkImageView& image, VkSampler sampler);
};

class VulkanEngine {
public:

    // singleton style getter.multiple engines is not supported
    static VulkanEngine& Get();

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();
    void drawMain(VkCommandBuffer cmd);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawGeometry(VkCommandBuffer cmd);

    // run main loop
    void run();

    void updateScene();

    // upload a mesh into a pair of gpu buffers. If descriptor allocator is not
    // null, it will also create a descriptor that points to the vertex buffer
    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    FrameData& getCurrentFrame();
    FrameData& getLastFrame();

    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    void destroyImage(const AllocatedImage& img);
    void destroyBuffer(const AllocatedBuffer& buffer);

public:

    bool _isInitialized = false;
    int _frameNumber = 0;

    VkExtent2D _windowExtent = VkExtent2D(1700, 900);

    struct SDL_Window* _window = nullptr;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    VkPhysicalDevice _chosenGPU;
    VkDevice _device;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    AllocatedBuffer _defaultGLTFMaterialData;

    FrameData _frames[FRAME_OVERLAP];

    VkSurfaceKHR _surface;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    VkExtent2D _drawExtent;
    VkDescriptorPool _descriptorPool;

    DescriptorAllocator _globalDescriptorAllocator;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    DeletionQueue _mainDeletionQueue;

    VmaAllocator _allocator; // vma lib allocator

    VkDescriptorSetLayout _gpu_sceneDataDescriptorLayout;

    GLTFMetallic_Roughness _metalRoughMaterial;

    // draw resources
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    TextureCache _texCache;

    GPUMeshBuffers _rectangle;
    DrawContext _drawCommands;

    GPU_sceneData _sceneData;

    Camera _mainCamera;

    EngineStats _engineStats;

    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackgroundEffect = 0;

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;
    std::vector<std::shared_ptr<LoadedGLTF>> _brickadiaScene;

    bool shouldResizeWindow = false;
    bool shouldFreezeRendering = false;

private:
    void initVulkan();

    void initSwapchain();

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

    void destroySwapchain();

    void initCommands();

    void initPipelines();

    void initBackgroundPipelines();

    void initDescriptors();

    void initSyncStructures();

    void initRenderables();

    void initImgui();

    void initDefaultData();
};
