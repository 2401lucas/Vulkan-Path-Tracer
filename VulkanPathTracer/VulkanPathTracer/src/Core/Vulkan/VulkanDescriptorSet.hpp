#pragma once
#include "VulkanDevice.h"

namespace core_internal::rendering {
// TODO: Clear bindings when createing descriptor set layout to support more
// descriptor set creation with one descriptor set
class VulkanDescriptorSet {
 private:
  VulkanDevice* vulkanDevice;

  VkDescriptorPool pool;

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;
  VkPipelineLayout pipelineLayout;

 public:
  explicit VulkanDescriptorSet(core_internal::rendering::VulkanDevice*);
  ~VulkanDescriptorSet();

  operator VkPipelineLayout() { return pipelineLayout; }

  VkDescriptorSet getSet(uint32_t);

  void addBinding(uint32_t binding, VkDescriptorType, uint32_t,
                  VkPipelineStageFlags, const VkSampler* = nullptr);
  void initLayout();
  VkDescriptorPool initPool(uint32_t);
  VkPipelineLayout initPipelineLayout(
      uint32_t numRanges = 0, const VkPushConstantRange* ranges = nullptr,
      VkPipelineLayoutCreateFlags flags = 0);
  void allocateDescriptorSets(VkDescriptorPool);

  VkWriteDescriptorSet makeWrite(VkDescriptorSet dstSet, uint32_t dstBinding,
                                 const VkDescriptorImageInfo* pImageInfo,
                                 uint32_t arrayElement = 0) const;

  VkWriteDescriptorSet makeWrite(VkDescriptorSet dstSet, uint32_t dstBinding,
                                 const VkDescriptorBufferInfo* pBufferInfo,
                                 uint32_t arrayElement = 0) const;
  VkWriteDescriptorSet makeWrite(
      VkDescriptorSet dstSet, uint32_t dstBinding,
      const VkWriteDescriptorSetAccelerationStructureKHR* pAccel,
      uint32_t arrayElement = 0) const;

  VkWriteDescriptorSet makeWrite(VkDescriptorSet dstSet, uint32_t dstBinding,
                                 uint32_t arrayElement = 0) const;
};
}  // namespace core_internal::rendering