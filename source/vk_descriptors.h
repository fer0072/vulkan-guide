#pragma once

#include <vector>
#include <vk_types.h>
#include <deque>
#include <span>

//> descriptor_layout
struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};
//< descriptor_layout
// 
//> writer
struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void addImageDescriptorSet(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
    void addBufferDescriptorSet(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 

    void clear();
    void updateDescriptorSets(VkDevice device, VkDescriptorSet set);
};
//< writer
// 
//> descriptor_allocator
struct DescriptorAllocator {

    struct PoolSizeRatio{
		VkDescriptorType type;
		float ratio;
    };

    VkDescriptorPool pool;

    void initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
//< descriptor_allocator

//> descriptor_allocator_grow
struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clearPools(VkDevice device);
	void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
	VkDescriptorPool getPool(VkDevice device);
	VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};
//< descriptor_allocator_grow