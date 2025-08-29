#pragma once

#include <vk_types.h>

class PipelineBuilder {
//> pipeline
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;

	PipelineBuilder(){ clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device);
//< pipeline
    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void disableMultisampling();
    void disableBlending();
    void enableBlendingAdditive();
    void enableBlendingAlphaBlend();

    void setColorAttachmentFormat(VkFormat format);
	void setDepthAttachmentFormat(VkFormat format);
	void disableDepthTest();
    void enableDepthTest(bool depthWriteEnable,VkCompareOp op);
};

namespace vkUtils {
bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
