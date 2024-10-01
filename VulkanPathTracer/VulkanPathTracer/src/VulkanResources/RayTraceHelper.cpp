#include "RayTraceHelper.hpp"

#include <algorithm>

namespace core_internal::rendering::raytracing {
RayTraceBuilder::RayTraceBuilder(
    core_internal::rendering::VulkanDevice* vulkanDevice) {
  this->vulkanDevice = vulkanDevice;
}

RayTraceBuilder::~RayTraceBuilder() {}

void RayTraceBuilder::buildBlas(const std::vector<BlasInput>& input,
                                VkBuildAccelerationStructureFlagsKHR flags) {
  auto numBlas = static_cast<uint32_t>(input.size());
  VkDeviceSize asTotalSize{0};
  VkDeviceSize maxScratchSize{0};

  std::vector<tools::AccelerationStructureBuildData> blasBuildData(numBlas);
  blas.resize(numBlas);

  for (uint32_t i = 0; i < numBlas; i++) {
    blasBuildData[i].asType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasBuildData[i].asGeometry = input[i].asGeometry;
    blasBuildData[i].asBuildRangeInfo = input[i].asBuildRangeInfo;

    auto sizeInfo = blasBuildData[i].finalizeGeometry(vulkanDevice,
                                                      input[i].asFlags | flags);
    maxScratchSize = std::max(maxScratchSize, sizeInfo.buildScratchSize);
  }

  VkDeviceSize hintMaxBudget{256000000};  // 256 MB

  bool hasCompaction =
      flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

  auto blasBuilder = new tools::BlasBuilder(vulkanDevice);
  uint32_t minAlignment =
      vulkanDevice->
      operator VkPhysicalDeviceAccelerationStructurePropertiesKHR()
          .minAccelerationStructureScratchOffsetAlignment;

  VkDeviceSize scratchSize =
      blasBuilder->getScratchSize(hintMaxBudget, blasBuildData, minAlignment);

  Buffer* blasScratchBuf = new Buffer();

  VkBufferCreateInfo blasScratchBufCI{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = scratchSize,
      .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
  };

  vulkanDevice->createBuffer(blasScratchBuf, blasScratchBufCI,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  std::vector<VkDeviceAddress> scratchAddresses;
  blasBuilder->getScratchAddresses(hintMaxBudget, blasBuildData,
                                   blasScratchBuf->deviceAddress,
                                   scratchAddresses, minAlignment);

  bool isFinished = false;
  do {
    {
      VkCommandBuffer cmd = vulkanDevice->createCommandBuffer();
      isFinished = blasBuilder->cmdCreateParallelBlas(
          cmd, blasBuildData, blas, scratchAddresses, hintMaxBudget);
      vkEndCommandBuffer(cmd);
      vulkanDevice->submitCommandBuffer(cmd);
      vulkanDevice->waitIdle();
    }
    if (hasCompaction) {
      VkCommandBuffer cmd = vulkanDevice->createCommandBuffer();
      blasBuilder->cmdCompactBlas(cmd, blasBuildData, blas);
      vkEndCommandBuffer(cmd);
      vulkanDevice->submitCommandBuffer(cmd);
      vulkanDevice->waitIdle();
      blasBuilder->destroyNonCompactedBlas();
    }
  } while (!isFinished);

  DEBUG_LOG(blasBuilder->getStatistics());

  // Clean up
  vulkanDevice->destroy(blasScratchBuf);
}
void RayTraceBuilder::buildTlas(
    const std::vector<VkAccelerationStructureInstanceKHR>& instances,
    VkBuildAccelerationStructureFlagsKHR flags, bool update) {
  assert(tlas.accel == VK_NULL_HANDLE || update);
  uint32_t countInstance = static_cast<uint32_t>(instances.size());
  VkCommandBuffer cmd = vulkanDevice->createCommandBuffer();

  Buffer instancesBuffer;

  VkBufferCreateInfo bufferCI{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = sizeof(VkAccelerationStructureInstanceKHR) * countInstance,
      .usage =
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
  };

  vulkanDevice->createBuffer(&instancesBuffer, bufferCI,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

uint64_t RayTraceBuilder::getBlasDeviceAddress(uint32_t blasId) {
  assert(size_t(blasId) < blas.size());
  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
  addressInfo.accelerationStructure = blas[blasId].accel;
  return pfnGetAccelerationStructureDeviceAddressKHR(
      vulkanDevice->operator VkDevice(), &addressInfo);
}
VkAccelerationStructureKHR RayTraceBuilder::getAccelerationStructure() {
  return tlas.accel;
}
}  // namespace core_internal::rendering::raytracing