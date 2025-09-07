#include "vk_engine.h"

#include "vk_images.h"
#include "vk_loader.h"
#include "vk_descriptors.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#ifdef _DEBUG
//#define VMA_DEBUG_LOG_FORMAT(format, ...)  printf((format), __VA_ARGS__)
//#define VMA_DEBUG_LOG(str)                 printf("%s\n", (str))
//#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
//#define VMA_DEBUG_DETECT_CORRUPTION 1
//#define VMA_DEBUG_MARGIN 16
//#define VMA_DEBUG_GLOBAL_MUTEX 1
#endif

constexpr bool bUseValidationLayers = true;

// we want to immediately abort when there is an error. In normal engines this
// would give an error message to the user, or perform a dump of state.
using namespace std;

VulkanEngine* loadedEngine = nullptr;

glm::vec4 normalizePlane(glm::vec4 p)
{
    return p / glm::length(glm::vec3(p));
}

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
        _windowExtent.height, window_flags);

    initVulkan();

    initSwapchain();

    initCommands();

    initSyncStructures();

    initDescriptors();

    initPipelines();

    createDefaultObjects();

    initScene();

    initRenderObjects();

    initImgui();

    _renderScene.mergeMeshes(this);

    _renderScene.buildBatches();

    // everything went fine
    _isInitialized = true;

    _mainCamera.velocity = glm::vec3(0.f);
    _mainCamera.position = glm::vec3(30.f, -00.f, -085.f);

    _mainCamera.pitch = 0;
    _mainCamera.yaw = 0;
}

void VulkanEngine::createDefaultObjects() {
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    AllocatedImage whiteImage = createImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
    _defaultImages["defaultWhiteImage"] = std::make_shared<AllocatedImage>(whiteImage);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    AllocatedImage greyImage = createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
    _defaultImages["defaultGreyImage"] = std::make_shared<AllocatedImage>(greyImage);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    AllocatedImage blackImage = createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
    _defaultImages["defaultBlackImage"] = std::make_shared<AllocatedImage>(blackImage);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

    AllocatedImage errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
    _defaultImages["defaulterrorCheckerboardImage"] = std::make_shared<AllocatedImage>(errorCheckerboardImage);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    VkSampler defaultSamplerNearest;
	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;   
	vkCreateSampler(_device, &sampl, nullptr, &defaultSamplerNearest);
    _defaultSamplers["defaultSamplerNearest"] = std::make_shared<VkSampler>(defaultSamplerNearest);

    VkSampler defaultSamplerLinear;
	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &defaultSamplerLinear);
    _defaultSamplers["defaultSamplerLinear"] = std::make_shared<VkSampler>(defaultSamplerLinear);

    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _defaultImages["defaultWhiteImage"]->imageView, nullptr);
        vmaDestroyImage(_allocator, _defaultImages["defaultWhiteImage"]->image, _defaultImages["defaultWhiteImage"]->allocation);

        vkDestroyImageView(_device, _defaultImages["defaultGreyImage"]->imageView, nullptr);
        vmaDestroyImage(_allocator, _defaultImages["defaultGreyImage"]->image, _defaultImages["defaultGreyImage"]->allocation);

        vkDestroyImageView(_device, _defaultImages["defaultBlackImage"]->imageView, nullptr);
        vmaDestroyImage(_allocator, _defaultImages["defaultBlackImage"]->image, _defaultImages["defaultBlackImage"]->allocation);

        vkDestroyImageView(_device, _defaultImages["defaulterrorCheckerboardImage"]->imageView, nullptr);
        vmaDestroyImage(_allocator, _defaultImages["defaulterrorCheckerboardImage"]->image, _defaultImages["defaulterrorCheckerboardImage"]->allocation);

        vkDestroySampler(_device, *_defaultSamplers["defaultSamplerNearest"].get(), nullptr);
        vkDestroySampler(_device, *_defaultSamplers["defaultSamplerLinear"].get(), nullptr);
        });
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {

        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_device);

        _loadedScenes.clear();

        for (auto& frame : _frames) {
            frame._deletionQueue.flush();
        }

        _mainDeletionQueue.flush();

        destroySwapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vmaDestroyAllocator(_allocator);

        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }
}

void VulkanEngine::initBackgroundEffects()
{
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

    VkPipelineLayout backgroundPipelineLayout;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &backgroundPipelineLayout));

	VkShaderModule gradientShader;
	if (!vkUtils::loadShaderModule("../../shaders/gradient_color.comp.spv", _device, &gradientShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkShaderModule skyShader;
	if (!vkUtils::loadShaderModule("../../shaders/sky.comp.spv", _device, &skyShader)) {
        fmt::print("Error when building the compute shader\n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = backgroundPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = backgroundPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};

	//default colors
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	//change the shader module only to create the sky shader
	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = backgroundPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	//default sky parameters
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	//add the 2 background effects into the array
	_backgroundEffects.push_back(gradient);
	_backgroundEffects.push_back(sky);

	//destroy structures properly
	vkDestroyShaderModule(_device, gradientShader, nullptr);
	vkDestroyShaderModule(_device, skyShader, nullptr);
	_mainDeletionQueue.push_function([&]() {
        vkDestroyPipeline(_device, _backgroundEffects[0].pipeline, nullptr);
        vkDestroyPipeline(_device, _backgroundEffects[1].pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _backgroundEffects[0].layout, nullptr);
		});
}

void VulkanEngine::initComputeCullEffect()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_cullDataDescriptorSetLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(DrawCullData);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VkPipelineLayout computeCullPipelineLayout;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &computeCullPipelineLayout));

    VkShaderModule computeCullShader;
    if (!vkUtils::loadShaderModule("../../shaders/indirect_cull.comp.spv", _device, &computeCullShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeCullShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = computeCullPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    _computeCullEffect.layout = computeCullPipelineLayout;
    _computeCullEffect.name = "computeCull";
    _computeCullEffect.data = {};

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_computeCullEffect.pipeline));

    // Destroy structures properly
    vkDestroyShaderModule(_device, computeCullShader, nullptr);
    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipeline(_device, _computeCullEffect.pipeline, nullptr);
        vkDestroyPipelineLayout(_device, _computeCullEffect.layout, nullptr);
        });
}


void VulkanEngine::drawMain(VkCommandBuffer cmd)
{
    /*
    *   Draw the background effects.
    */
	ComputeEffect& effect = _backgroundEffects[_currentBackgroundEffect];

	// bind the background compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, uint32_t(std::ceil(_drawExtent.width / 16.0)), uint32_t(std::ceil(_drawExtent.height / 16.0)), 1);

    /*
    *  Prepare data for each passes.
    */
    _renderScene.prepareMeshData(cmd, this);
    _renderScene.prepareComputeCullData(cmd, this);

    computeCullPass(cmd);

	/*
	*  Draw the forward pass, including opaque objects and transparent objects.
    */   

    //shadowPass(cmd);
    forwardPass(cmd);
}

void VulkanEngine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkInit::colorAttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingInfo renderInfo = vkInit::renderingInfo(_windowExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VulkanEngine::updateSceneData()
{
    _mainCamera.updateCamera(_engineStats.frameTime);

    glm::mat4 view = _mainCamera.getViewMatrix();

    glm::mat4 projection = _mainCamera.getProjectionMatrix();

    _sceneData.view = view;
    _sceneData.proj = projection;
    _sceneData.viewproj = projection * view;
    _sceneData.sunlightDirection = glm::vec4(0.3f, 1.f, 0.3f, 1.0f);
    _sceneData.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    _sceneData.ambientColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void VulkanEngine::draw()
{
	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._renderFence, true, 1000000000));

	getCurrentFrame()._deletionQueue.flush();
    getCurrentFrame()._frameDescriptors.clearPools(_device);
	//request image from the swapchain
	uint32_t swapchainImageIndex;

	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        _shouldResizeWindow = true;
		return ;
	}
	_drawExtent.height = uint32_t(std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * 1.f);
	_drawExtent.width = uint32_t(std::min(_swapchainExtent.width, _drawImage.imageExtent.width) *  1.f);

	VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame()._renderFence));

    updateSceneData();

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(getCurrentFrame()._mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    _renderScene.buildBatches();










	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkUtils::imageLayoutTransition(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkUtils::imageLayoutTransition(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	drawMain(cmd);

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkUtils::imageLayoutTransition(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkUtils::imageLayoutTransition(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkExtent2D extent;
	extent.height = _windowExtent.height;
	extent.width = _windowExtent.width;
	//extent.depth = 1;

	// execute a copy from the draw image into the swapchain
	vkUtils::copyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent,_swapchainExtent);

	// set swapchain image layout to Attachment Optimal so we can draw it
	vkUtils::imageLayoutTransition(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//draw imgui into the swapchain image
	drawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can draw it
	vkUtils::imageLayoutTransition(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkInit::commandBufferSubmitInfo(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);

	VkSubmitInfo2 submit = vkInit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkInit::presentInfo();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        _shouldResizeWindow = true;
        return;
	}
	//increase the number of frames drawn
	_frameNumber++;
}

bool isVisible(const RenderObject& obj, const glm::mat4& viewproj) {
    std::array<glm::vec3, 8> corners {
        glm::vec3 { 1, 1, 1 },
        glm::vec3 { 1, 1, -1 },
        glm::vec3 { 1, -1, 1 },
        glm::vec3 { 1, -1, -1 },
        glm::vec3 { -1, 1, 1 },
        glm::vec3 { -1, 1, -1 },
        glm::vec3 { -1, -1, 1 },
        glm::vec3 { -1, -1, -1 },
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    bool anyPointInFront = false;
    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        if (v.w > 0.0f)
        {
            anyPointInFront = true;

            // perspective correction
            v.x = v.x / v.w;
            v.y = v.y / v.w;
            v.z = v.z / v.w;

            min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
            max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
        }
    }

    if (!anyPointInFront)return false;

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}

void VulkanEngine::generateComputeCullCommands(VkCommandBuffer cmd, RenderScene::MeshPass& meshPass, CullParams& cullParams)
{
    if (meshPass.indirectBatches.size() == 0) return;

    _cullDataDescriptorSet = getCurrentFrame()._frameDescriptors.allocate(_device, _cullDataDescriptorSetLayout);

    DescriptorWriter writer;
    writer.addBufferDescriptorSet(0, _renderScene.objectDataBuffer.value().buffer, _renderScene.objectDataBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writer.addBufferDescriptorSet(1, meshPass.drawIndirectBuffer.value().buffer, meshPass.drawIndirectBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writer.addBufferDescriptorSet(2, meshPass.GPUInstanceBuffer.value().buffer, meshPass.GPUInstanceBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writer.addBufferDescriptorSet(3, meshPass.compactedInstanceBuffer.value().buffer, meshPass.compactedInstanceBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    //TBD
    //writer.addImageDescriptorSet(4, _depthPyramid.imageView, _depthSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.addBufferDescriptorSet(4, getCurrentFrame()._sceneDataBuffer.buffer, getCurrentFrame()._sceneDataBuffer.size, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    writer.updateDescriptorSets(_device, _cullDataDescriptorSet);

    glm::mat4 projMat = cullParams.projMat;
    glm::mat4 transposedProjMat = glm::transpose(projMat);

    glm::vec4 frustumX = normalizePlane(transposedProjMat[3] + transposedProjMat[0]); // x + w < 0
    glm::vec4 frustumY = normalizePlane(transposedProjMat[3] + transposedProjMat[1]); // y + w < 0

    DrawCullData cullData = {};
    cullData.P00 = projMat[0][0];
    cullData.P11 = projMat[1][1];
    cullData.zNear = 0.1f;
    cullData.zFar = cullParams.drawDist;
    cullData.frustum[0] = frustumX.x;
    cullData.frustum[1] = frustumX.z;
    cullData.frustum[2] = frustumY.y;
    cullData.frustum[3] = frustumY.z;
    cullData.drawCount = static_cast<uint32_t>(meshPass.flatBatches.size());
    cullData.cullingEnabled = cullParams.frustrumCull;
    cullData.lodEnabled = false;
    cullData.occlusionEnabled = cullParams.occlusionCull;
    cullData.lodBase = 10.f;
    cullData.lodStep = 1.5f;
    //TBD
    cullData.pyramidWidth = 1.0f;// static_cast<float>(depthPyramidWidth);
    cullData.pyramidHeight = 1.0f;// static_cast<float>(depthPyramidHeight);
    cullData.viewMat = cullParams.viewMat;//get_view_matrix();

    cullData.AABBcheck = cullParams.aabb;
    cullData.aabbMin_x = cullParams.aabbMin.x;
    cullData.aabbMin_y = cullParams.aabbMin.y;
    cullData.aabbMin_z = cullParams.aabbMin.z;

    cullData.aabbMax_x = cullParams.aabbMax.x;
    cullData.aabbMax_y = cullParams.aabbMax.y;
    cullData.aabbMax_z = cullParams.aabbMax.z;

    if (cullParams.drawDist > 10000)
    {
        cullData.distanceCheck = false;
    }
    else
    {
        cullData.distanceCheck = true;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computeCullEffect.pipeline);

    vkCmdPushConstants(cmd, _computeCullEffect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DrawCullData), &cullData);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computeCullEffect.layout, 0, 1, &_cullDataDescriptorSet, 0, nullptr);

    vkCmdDispatch(cmd, static_cast<uint32_t>((meshPass.flatBatches.size() / 256) + 1), 1, 1);

    // Add memory barriers.
    {
        VkBufferMemoryBarrier barrier = vkInit::bufferMemoryBarrier(meshPass.compactedInstanceBuffer.value().buffer, _graphicsQueueFamily, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

        VkBufferMemoryBarrier barrier2 = vkInit::bufferMemoryBarrier(meshPass.drawIndirectBuffer.value().buffer, _graphicsQueueFamily, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

        postCullBarriers.emplace_back(barrier);
        postCullBarriers.emplace_back(barrier2);
    }
}

void VulkanEngine::computeCullPass(VkCommandBuffer cmd)
{
    postCullBarriers.clear();

    CullParams forwardCullParams;
    forwardCullParams.viewMat = _mainCamera.getViewMatrix();
	forwardCullParams.projMat = _mainCamera.getProjectionMatrix();
    forwardCullParams.frustrumCull = true;
    forwardCullParams.occlusionCull = true;
    // TBD use cvar to control
    forwardCullParams.drawDist = 5000.0f;
	forwardCullParams.aabb = false;

	generateComputeCullCommands(cmd, _renderScene.forwardOpaquePass, forwardCullParams);
    //generateComputeCullCommands(cmd, _renderScene.forwardTransparentPass, forwardCullParams);

    if (postCullBarriers.size() > 0)
    {
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
            0, nullptr, static_cast<uint32_t>(postCullBarriers.size()), postCullBarriers.data(), 0, nullptr);
	}
}

void VulkanEngine::shadowPass(VkCommandBuffer cmd)
{

}

void VulkanEngine::generateDrawCommands(VkCommandBuffer cmd, RenderScene::MeshPass& meshPass)
{
    //TBD
    if (meshPass.indirectBatches.size() > 0)
    {
        DrawMesh* lastMesh = nullptr;
        VkPipeline lastPipeline = VK_NULL_HANDLE;
        VkPipelineLayout lastLayout = VK_NULL_HANDLE;
        VkDescriptorSet lastMaterialSet = VK_NULL_HANDLE;

        for (int i = 0; i < meshPass.multiBatches.size(); i++)
        {
            auto& multiBatch = meshPass.multiBatches[i];
            auto& indirectBatch = meshPass.indirectBatches[multiBatch.first];

            VkPipeline newPipeline = indirectBatch.getMaterial()->pipeline->pipeline;
            VkPipelineLayout newLayout = indirectBatch.getMaterial()->pipeline->layout;
            VkDescriptorSet newDescriptorSet = indirectBatch.getMaterial()->materialSet;

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newPipeline);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 0, 1, &_globalDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 1, 1, &_objectDataDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newLayout, 2, 1, &newDescriptorSet, 0, nullptr);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &_renderScene.mergedVertexBuffer.value().buffer, &offset);
            vkCmdBindIndexBuffer(cmd, _renderScene.mergedIndexBuffer.value().buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexedIndirect(cmd, meshPass.drawIndirectBuffer.value().buffer, multiBatch.first * sizeof(GPUIndirectObject), multiBatch.count, sizeof(GPUIndirectObject));

            _engineStats.drawcallCount++;
        }
    }
}

void VulkanEngine::forwardPass(VkCommandBuffer cmd)
{
    /*
    *  Init stats data of the forward pass.
    */
    auto start = std::chrono::system_clock::now();

    _engineStats.drawcallCount = 0;
    _engineStats.triangleCount = 0;

    /*
    *  Record draw commands of the forward pass.
    */
    VkRenderingAttachmentInfo colorAttachment = vkInit::colorAttachmentInfo(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkInit::depthAttachmentInfo(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkInit::renderingInfo(_drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)_drawExtent.width;
    viewport.height = (float)_drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    std::vector<uint32_t> visibleOpaqueRenderObjects;
    visibleOpaqueRenderObjects.reserve(_renderScene.forwardOpaquePass.flatBatches.size());

    /*
    * Update the descriptors of scene data buffer, textures, and object data buffer.
    */ 
    VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr };
    uint32_t descriptorCounts = uint32_t(_texCache.cache.size()); // texture array and scene data buffer.
    allocArrayInfo.pDescriptorCounts = &descriptorCounts;
    allocArrayInfo.descriptorSetCount = 1;
    _globalDescriptorSet = getCurrentFrame()._frameDescriptors.allocate(_device, _globalDescriptorSetLayout, &allocArrayInfo);

	// Add scene date buffer to the global descriptor set.
    GPU_sceneData* sceneUniformData = (GPU_sceneData*)mapBuffer(getCurrentFrame()._sceneDataBuffer);
    *sceneUniformData = _sceneData;
    unmapBuffer(getCurrentFrame()._sceneDataBuffer);

    DescriptorWriter writer;
    writer.addBufferDescriptorSet(0, getCurrentFrame()._sceneDataBuffer.buffer, sizeof(GPU_sceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// Add texture array to the global descriptor set.
    if (_texCache.cache.size() > 0) {
        VkWriteDescriptorSet arraySet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        arraySet.descriptorCount = uint32_t(_texCache.cache.size());
        arraySet.dstArrayElement = 0;
        arraySet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        arraySet.dstBinding = 1;
        arraySet.pImageInfo = _texCache.cache.data();
        writer.writes.push_back(arraySet);
    }

    writer.updateDescriptorSets(_device, _globalDescriptorSet);

	// Add object data buffer to the global descriptor set.
    _objectDataDescriptorSet = getCurrentFrame()._frameDescriptors.allocate(_device, _objectDataDescriptorSetLayout);

    writer.clear();

    writer.addBufferDescriptorSet(0, _renderScene.objectDataBuffer.value().buffer, _renderScene.objectDataBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writer.addBufferDescriptorSet(1, _renderScene.forwardOpaquePass.compactedInstanceBuffer.value().buffer, _renderScene.forwardOpaquePass.compactedInstanceBuffer.value().size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writer.updateDescriptorSets(_device, _objectDataDescriptorSet);

    /*
	*  Add draw commands to the command buffer.
    */
    generateDrawCommands(cmd, _renderScene.forwardOpaquePass);
    //generateDrawCommands(cmd, _renderScene.forwardTransparentPass);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    _engineStats.forwardPassTime = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        auto start = std::chrono::system_clock::now();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {

				if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    _shouldResizeWindow = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					_shouldFreezeRendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					_shouldFreezeRendering = false;
				}
            }
            
            ImGui_ImplSDL2_ProcessEvent(&e);
            _mainCamera.processInputEvent(&e);
        }

        if (_shouldFreezeRendering) continue;

		if (_shouldResizeWindow) {
			resizeSwapchain();
		}

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        if (ImGui::Begin("Stats"))
        {
            ImGui::Text("frameTime %.2f ms", _engineStats.frameTime);
            ImGui::Text("shadow pass drawtime %.2f ms", _engineStats.shadowPassTime);
            ImGui::Text("foward pass drawtime %.2f ms", _engineStats.forwardPassTime);
            ImGui::Text("forwardTransparent pass drawtime %.2f ms", _engineStats.transparentPassTime);
            ImGui::Text("triangles %i", _engineStats.triangleCount);
            ImGui::Text("draws %i", _engineStats.drawcallCount);
        }
        ImGui::End();

		if (ImGui::Begin("background")) {

			ComputeEffect& selected = _backgroundEffects[_currentBackgroundEffect];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index", &_currentBackgroundEffect, 0, uint32_t(_backgroundEffects.size()) - 1);

			ImGui::InputFloat4("data1", (float*)&selected.data.data1);
			ImGui::InputFloat4("data2", (float*)&selected.data.data2);
			ImGui::InputFloat4("data3", (float*)&selected.data.data3);
			ImGui::InputFloat4("data4", (float*)&selected.data.data4);
		}
        ImGui::End();

		ImGui::Render();

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        _engineStats.frameTime = elapsed.count() / 1000.f;
    }
}

void VulkanEngine::initRenderObjects()
{
    if (_loadedScenes.count("structure"))
    {
        _loadedScenes["structure"]->generateRenderObject(glm::mat4{ 1.f }, _renderScene);
    }
}

AllocatedBuffer VulkanEngine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    newBuffer.size = allocSize;

    return newBuffer;
}

void* VulkanEngine::mapBuffer(const AllocatedBuffer& buffer)
{
    void* data;
    vmaMapMemory(_allocator, buffer.allocation, &data);
    return data;
}

void VulkanEngine::unmapBuffer(const AllocatedBuffer& buffer)
{
	vmaUnmapMemory(_allocator, buffer.allocation);
}

AllocatedImage VulkanEngine::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imgInfo = vkInit::imageCreateInfo(format, usage, size);
    if (mipmapped) {
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkInit::imageViewCreateInfo(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage VulkanEngine::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) {
        vkUtils::imageLayoutTransition(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped) {
            vkUtils::generateImageMipmaps(cmd, newImage.image,VkExtent2D{newImage.imageExtent.width,newImage.imageExtent.height});
        } else {
            vkUtils::imageLayoutTransition(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    getCurrentFrame()._deletionQueue.push_function([=]() {
        destroyBuffer(uploadbuffer);
		});
    
    return newImage;
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

	newSurface.original = std::make_shared<OriginalMesh>();
    newSurface.original->_indices = std::vector(indices.begin(), indices.end());
    newSurface.original->_vertices = std::vector(vertices.begin(), vertices.end());
    
    newSurface.vertexBuffer = createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    
    // TBD 不indirect draw的话就设置现在这个flag，要indirect draw的话就设置VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    //newSurface.indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    newSurface.indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = mapBuffer(staging);
    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
    unmapBuffer(staging);

    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy { 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy { 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    getCurrentFrame()._deletionQueue.push_function([=]() {
        destroyBuffer(staging);
        });

    return newSurface;
}

FrameData& VulkanEngine::getCurrentFrame()
{
    return _frames[_frameNumber % FRAME_OVERLAP];
}

FrameData& VulkanEngine::getLastFrame()
{
    return _frames[(_frameNumber - 1) % FRAME_OVERLAP];
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkInit::commandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submit = vkInit::submitInfo(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::destroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void VulkanEngine::destroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::initVulkan()
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    VkPhysicalDeviceFeatures features{};
    features.multiDrawIndirect = true;
	features.drawIndirectFirstInstance = true;

    VkPhysicalDeviceVulkan13Features features13 {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;
    
    VkPhysicalDeviceVulkan12Features features12 {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true; 
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.runtimeDescriptorArray = true;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.2
    vkb::PhysicalDeviceSelector selector { vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3).set_required_features_13(features13).set_required_features_12(features12).set_required_features(features).set_surface(_surface).select().value();

    // physicalDevice.features.
    // create the final vulkan device

    vkb::DeviceBuilder deviceBuilder { physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::initSwapchain()
{
    createSwapchain(_windowExtent.width, _windowExtent.height);

	//depth image size will match the window
	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	//hardcoding the draw format to 32 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rimg_info = vkInit::imageCreateInfo(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkInit::imageViewCreateInfo(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    //create a depth image too
	//hardcoding the draw format to 32 bit float
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkInit::imageCreateInfo(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dview_info = vkInit::imageViewCreateInfo(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));


	//add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
	});
}


void VulkanEngine::createSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}
void VulkanEngine::destroySwapchain()
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::resizeSwapchain()
{
	vkDeviceWaitIdle(_device);

	destroySwapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	createSwapchain(_windowExtent.width, _windowExtent.height);

	_shouldResizeWindow = false;
}

void VulkanEngine::initCommands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkInit::commandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {

        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::commandBufferAllocateInfo(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        _mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr); });
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::commandBufferAllocateInfo(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}

void VulkanEngine::initSyncStructures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo = vkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));

    _mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });

    for (int i = 0; i < FRAME_OVERLAP; i++) {

        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = vkInit::semaphoreCreateInfo();

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

        _mainDeletionQueue.push_function([=]() {
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
        });
    }
}

void VulkanEngine::initScene()
{
    // Setup camera and init the scene data.
    _mainCamera.updateCamera(_engineStats.frameTime);

    glm::mat4 view = _mainCamera.getViewMatrix();

    glm::mat4 projection = _mainCamera.getProjectionMatrix();

    _sceneData.view = view;
	_sceneData.proj = projection;
	_sceneData.viewproj = projection * view;
    _sceneData.sunlightDirection = glm::vec4(0.3f, 1.f, 0.3f, 1.0f);
    _sceneData.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    _sceneData.ambientColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Load models.
    std::string structurePath = { "..\\..\\assets\\structure.glb" };
    auto structureFile = loadGltf(this, structurePath);
    assert(structureFile.has_value());
    _loadedScenes["structure"] = *structureFile;

    // Create global buffers for scene data.
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        //allocate a new uniform buffer for the scene data
        _frames[i]._sceneDataBuffer = createBuffer(sizeof(GPU_sceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        //add it to the deletion queue of this frame so it gets deleted once its been used
        _mainDeletionQueue.push_function([=, this]() {
            destroyBuffer(_frames[i]._sceneDataBuffer);
            });
    }
}

void VulkanEngine::initImgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
    poolInfo.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &imguiPool));

    // 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo imGuiInitInfo = {};
	imGuiInitInfo.Instance = _instance;
	imGuiInitInfo.PhysicalDevice = _chosenGPU;
	imGuiInitInfo.Device = _device;
	imGuiInitInfo.Queue = _graphicsQueue;
	imGuiInitInfo.DescriptorPool = imguiPool;
	imGuiInitInfo.MinImageCount = 3;
	imGuiInitInfo.ImageCount = 3;
	imGuiInitInfo.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	imGuiInitInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	imGuiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	imGuiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;


	imGuiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imGuiInitInfo);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		});
}

void VulkanEngine::initPipelines()
{
    // COMPUTE PIPELINES
    initBackgroundEffects();
    initComputeCullEffect();

    _metalRoughMaterial.buildPipelines(this);
}

void VulkanEngine::initDescriptors()
{
    // create a descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
    };

    _globalDescriptorAllocator.initPool(_device, 10, sizes);
    _mainDeletionQueue.push_function(
        [&]() { vkDestroyDescriptorPool(_device, _globalDescriptorAllocator.pool, nullptr); });

    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        builder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        builder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        builder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        //builder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
        builder.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
        _cullDataDescriptorSetLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
        builder.addBinding(1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4048);
               
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext = nullptr};

        std::array<VkDescriptorBindingFlags,2> flagArray { 0,VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT  };
        
        bindFlags.bindingCount = 2;
        bindFlags.pBindingFlags = flagArray.data();

        _globalDescriptorSetLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &bindFlags);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        builder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        _objectDataDescriptorSetLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT);
    }

    _mainDeletionQueue.push_function([&]() {
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _globalDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _objectDataDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _cullDataDescriptorSetLayout, nullptr);
    });

    _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);
    {
        DescriptorWriter writer;	
		writer.addImageDescriptorSet(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.updateDescriptorSets(_device, _drawImageDescriptors);
    }

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroyPools(_device);
		});
	}
}

void GLTFMetallic_Roughness::buildPipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (!vkUtils::loadShaderModule("../../shaders/tri_mesh_ssbo.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkUtils::loadShaderModule("../../shaders/tri_mesh_ssbo.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

    materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_globalDescriptorSetLayout, engine->_objectDataDescriptorSetLayout, 
        materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkInit::pipelineLayoutCreateInfo();
	mesh_layout_info.setLayoutCount = 3;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;

	pipelineBuilder.setShaders(meshVertexShader, meshFragShader);

	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);

	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

	pipelineBuilder.disableMultisampling();

	pipelineBuilder.disableBlending();

	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.setColorAttachmentFormat(engine->_drawImage.imageFormat);
	pipelineBuilder.setDepthAttachmentFormat(engine->_depthImage.imageFormat);

    pipelineBuilder.setVertexDescription();

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.buildPipeline(engine->_device);

	// create the forwardTransparent variant
	pipelineBuilder.enableBlendingAdditive();

	pipelineBuilder.enableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.buildPipeline(engine->_device);
	
	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);

    engine->_mainDeletionQueue.push_function([=]() {
        vkDestroyPipeline(engine->_device, opaquePipeline.pipeline, nullptr);
        vkDestroyPipeline(engine->_device, transparentPipeline.pipeline, nullptr);
        vkDestroyPipelineLayout(engine->_device, newLayout, nullptr);
        vkDestroyDescriptorSetLayout(engine->_device, materialLayout, nullptr);
		});
}

void GLTFMetallic_Roughness::clearResources(VkDevice device)
{

}

MaterialInstance GLTFMetallic_Roughness::updateMaterialDescriptorSets(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::forwardTransparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device,materialLayout);
    
    writer.clear();
    writer.addBufferDescriptorSet(0,resources.dataBuffer,sizeof(MaterialConstants),resources.dataBufferOffset,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    writer.updateDescriptorSets(device,matData.materialSet);

    return matData;
}

void MeshNode::generateRenderObject(const glm::mat4& topMatrix, RenderScene& scene)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (GeoSurface& s : mesh->surfaces) {
        RenderObject newObject;
        newObject.material = s.material;
        newObject.bounds = s.bounds;
        newObject.transform = nodeMatrix;
        newObject.meshID = scene.getMeshHandle(&s, mesh);
        newObject.passIndices.clear(-1);
        Handle<RenderObject> handle;
        handle.handle = static_cast<uint32_t>(scene.allRenderObjects.size());

        scene.allRenderObjects.push_back(newObject);

        if (s.material->passType == MaterialPass::forwardTransparent) 
        {
            scene.forwardTransparentPass.unbatchedObjects.push_back(handle);
        }
        else 
        {
			scene.forwardOpaquePass.unbatchedObjects.push_back(handle);
			scene.shadowPass.unbatchedObjects.push_back(handle);
        }
    }
       
    // recurse down
    Node::generateRenderObject(topMatrix, scene);
}


TextureID TextureCache::addTexture(const VkImageView& image, VkSampler sampler)
{
    for (unsigned int i = 0; i < cache.size(); i++) {
        if (cache[i].imageView == image && cache[i].sampler == sampler) {
            //found, return it
            return TextureID{i};
        }
    }

	uint32_t idx = uint32_t(cache.size());

	cache.push_back(VkDescriptorImageInfo{ .sampler = sampler,.imageView = image, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

	return TextureID{ idx };
}
