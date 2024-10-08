#include "VulkanDescriptorSet.hpp"

#include "../Tools/HelperMacros.hpp"

core_internal::rendering::VulkanDescriptorSet::VulkanDescriptorSet(
    VulkanDevice* device)
    : vulkanDevice(device) {}

core_internal::rendering::VulkanDescriptorSet::~VulkanDescriptorSet() {}

VkDescriptorSet core_internal::rendering::VulkanDescriptorSet::getSet(
    uint32_t) {
  return set;
}

void core_internal::rendering::VulkanDescriptorSet::addBinding(
    uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount,
    VkPipelineStageFlags stageFlags, const VkSampler* pImmutableSamplers) {
  bindings.push_back({binding, descriptorType, descriptorCount, stageFlags,
                      pImmutableSamplers});
}

void core_internal::rendering::VulkanDescriptorSet::initLayout() {
  assert(layout == VK_NULL_HANDLE);

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->operator VkDevice(),
                                              &descriptorSetLayoutCI, nullptr,
                                              &layout));
}

VkDescriptorPool core_internal::rendering::VulkanDescriptorSet::initPool(
    uint32_t numSets) {
  assert(pool == VK_NULL_HANDLE);
  assert(layout);
  uint32_t storageBufferCount = 0, uniformBufferCount = 0,
           combinedImageSamplerCount = 0, accelerationStructureCount = 0;

  for (auto& binding : bindings) {
    switch (binding.descriptorType) {
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        storageBufferCount++;
        break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        uniformBufferCount++;
        break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        combinedImageSamplerCount++;
        break;
      case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        accelerationStructureCount++;
        break;
      default:
        DEBUG_ERROR("Missing descriptor type implementation: " +
                    binding.descriptorType);
        break;
    }
  }

  std::vector<VkDescriptorPoolSize> poolSizes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBufferCount * numSets},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       combinedImageSamplerCount * numSets},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBufferCount * numSets},
      {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
       accelerationStructureCount * numSets},
  };

  VkDescriptorPoolCreateInfo descriptorPoolCI{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = uniformBufferCount + combinedImageSamplerCount +
                 storageBufferCount + accelerationStructureCount,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice->operator VkDevice(),
                                         &descriptorPoolCI, nullptr, &pool));

  allocateDescriptorSets(pool);
  return pool;
}

VkPipelineLayout
core_internal::rendering::VulkanDescriptorSet::initPipelineLayout(
    uint32_t numRanges, const VkPushConstantRange* ranges,
    VkPipelineLayoutCreateFlags flags) {
  assert(pipelineLayout == VK_NULL_HANDLE);
  assert(layout);

  VkPipelineLayoutCreateInfo layoutCreateInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layoutCreateInfo.flags = flags;
  layoutCreateInfo.pushConstantRangeCount = numRanges;
  layoutCreateInfo.pPushConstantRanges = ranges;
  layoutCreateInfo.setLayoutCount = 1;
  layoutCreateInfo.pSetLayouts = &layout;

  VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->operator VkDevice(),
                                         &layoutCreateInfo, nullptr,
                                         &pipelineLayout));
  return pipelineLayout;
}

void core_internal::rendering::VulkanDescriptorSet::allocateDescriptorSets(
    VkDescriptorPool pool) {
  assert(pool);

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout,
  };

  vkAllocateDescriptorSets(vulkanDevice->operator VkDevice(), &allocInfo, &set);
}

VkWriteDescriptorSet core_internal::rendering::VulkanDescriptorSet::makeWrite(
    VkDescriptorSet dstSet, uint32_t dstBinding,
    const VkDescriptorImageInfo* pImageInfo, uint32_t arrayElement) const {
  VkWriteDescriptorSet writeSet = makeWrite(dstSet, dstBinding, arrayElement);
  assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

  writeSet.pImageInfo = pImageInfo;
  return writeSet;
}

VkWriteDescriptorSet core_internal::rendering::VulkanDescriptorSet::makeWrite(
    VkDescriptorSet dstSet, uint32_t dstBinding,
    const VkDescriptorBufferInfo* pBufferInfo, uint32_t arrayElement) const {
  VkWriteDescriptorSet writeSet = makeWrite(dstSet, dstBinding, arrayElement);
  assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         writeSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

  writeSet.pBufferInfo = pBufferInfo;
  return writeSet;
}

VkWriteDescriptorSet core_internal::rendering::VulkanDescriptorSet::makeWrite(
    VkDescriptorSet dstSet, uint32_t dstBinding,
    const VkWriteDescriptorSetAccelerationStructureKHR* pAccel,
    uint32_t arrayElement) const {
  VkWriteDescriptorSet writeSet = makeWrite(dstSet, dstBinding, arrayElement);
  assert(writeSet.descriptorType ==
         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

  writeSet.pNext = pAccel;
  return writeSet;
}

VkWriteDescriptorSet core_internal::rendering::VulkanDescriptorSet::makeWrite(
    VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t arrayElement) const {
  VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  writeSet.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  for (size_t i = 0; i < bindings.size(); i++) {
    if (bindings[i].binding == dstBinding) {
      writeSet.descriptorCount = 1;
      writeSet.descriptorType = bindings[i].descriptorType;
      writeSet.dstBinding = dstBinding;
      writeSet.dstSet = dstSet;
      writeSet.dstArrayElement = arrayElement;
      return writeSet;
    }
  }
  assert(0 && "binding not found");
  return writeSet;
}
