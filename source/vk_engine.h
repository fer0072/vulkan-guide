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
#include <vk_scene.h>

constexpr unsigned int FRAME_OVERLAP = 2;

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

//> Compute cull related data.
struct CullParams {
    glm::mat4 viewMat;
    glm::mat4 projMat;
    bool occlusionCull;
    bool frustrumCull;
    float drawDist;
    bool aabb;
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
};

struct DrawCullData
{
    glm::mat4 viewMat;
    float P00, P11, zNear, zFar; // symmetric projection parameters
    float frustum[4]; // data for left/right/top/bottom frustum planes
    float lodBase, lodStep; // lod distance i = base * pow(step, i)
    float pyramidWidth, pyramidHeight; // depth pyramid size in texels

    uint32_t drawCount;

    int cullingEnabled;
    int lodEnabled;
    int occlusionEnabled;
    int distanceCheck;
    int AABBcheck;
    float aabbMin_x;
    float aabbMin_y;
    float aabbMin_z;
    float aabbMax_x;
    float aabbMax_y;
    float aabbMax_z;

};
//< Compute cull related data.

struct FrameData {
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    AllocatedBuffer _sceneDataBuffer;
};

struct EngineStats {
    float frameTime;
    int triangleCount;
    int drawcallCount;
    float shadowPassTime = 0.0f;
    float forwardPassTime = 0.0f;
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

    // run main loop
    void run();

    // shuts down the engine
    void cleanup();

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    FrameData& getCurrentFrame();
    FrameData& getLastFrame();

    const VkDevice& getDevice() const { return _device; }

    const uint32_t getGraphicsQueueFamily() const { return _graphicsQueueFamily; }

    const VmaAllocator getAllocator() const { return _allocator; }

public:
    DeletionQueue _mainDeletionQueue;

    //> draw resources
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

    std::unordered_map<std::string, std::shared_ptr<AllocatedImage>> _defaultImages;
    std::unordered_map<std::string, std::shared_ptr<VkSampler>> _defaultSamplers;

    VkDescriptorSetLayout _globalDescriptorSetLayout;
    VkDescriptorSetLayout _objectDataDescriptorSetLayout;

    GLTFMetallic_Roughness _metalRoughMaterial;

    TextureCache _texCache;
    //< draw resources    

private:
	//> Init behaviours.
    void initVulkan();

    void initSwapchain();

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

    void initCommands();

    void initPipelines();

    void initBackgroundEffects();

    void initComputeCullEffect();

    void initDescriptors();

    void initSyncStructures();

    void initScene();

    void initImgui();

    void initRenderObjects();

    void createDefaultObjects();
	//< Init behaviours.

    void destroySwapchain();

	//> Draw related behaviours.
    // draw loop
    void draw();

    void updateSceneData();

    void drawMain(VkCommandBuffer cmd);

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

    void generateComputeCullCommands(VkCommandBuffer cmd, RenderScene::MeshPass& meshPass, CullParams& cullParams);

    void computeCullPass(VkCommandBuffer cmd);

    void generateDrawCommands(VkCommandBuffer cmd, RenderScene::MeshPass& meshPass);

    void shadowPass(VkCommandBuffer cmd);

    void forwardPass(VkCommandBuffer cmd);
	//< Draw related behaviours.

private:

    bool _isInitialized = false;
    int _frameNumber = 0;

    bool _shouldResizeWindow = false;
    bool _shouldFreezeRendering = false;

    VkExtent2D _windowExtent = VkExtent2D(1700, 900);

    struct SDL_Window* _window = nullptr;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    VkPhysicalDevice _chosenGPU;
    VkDevice _device;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    FrameData _frames[FRAME_OVERLAP];

    VkSurfaceKHR _surface;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    VkExtent2D _drawExtent;
    VkDescriptorPool _descriptorPool;

    VmaAllocator _allocator; // vma lib allocator
    DescriptorAllocator _globalDescriptorAllocator;
    
    RenderScene _renderScene;
    GPU_sceneData _sceneData;
    Camera _mainCamera;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackgroundEffect = 0;
    ComputeEffect _computeCullEffect;

    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _cullDataDescriptorSetLayout;
    VkDescriptorSet _cullDataDescriptorSet;
    
    VkDescriptorSet _globalDescriptorSet;
    VkDescriptorSet _objectDataDescriptorSet;

    std::vector<VkBufferMemoryBarrier> postCullBarriers;

    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    EngineStats _engineStats;    

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;
};
