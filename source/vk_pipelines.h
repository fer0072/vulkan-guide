#pragma once

#include <vk_types.h>

class PipelineBuilder {
//> pipeline
public:
    struct VertexInputDescription {
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;

        VkPipelineVertexInputStateCreateFlags flags = 0;
    };

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VertexInputDescription _vertexInputDescription;
    VkFormat _colorAttachmentformat;

	PipelineBuilder(){ clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device);
//< pipeline
    void setVertexDescription();
    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void disableMultisampling();
    void disableBlending();
    void enableBlendingAdditive();
    void enableBlendingAlphaBlend();

    void setColorAttachmentFormat(VkFormat format);
    void disableColorAttachement();
	void setDepthAttachmentFormat(VkFormat format);
	void disableDepthTest();
    void enableDepthTest(bool depthWriteEnable,VkCompareOp op);
};

namespace vkUtils {
bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
