#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

#include "../Core/Tools/HelperMacros.hpp"
#include "../Core/Vulkan/VulkanDevice.h"

#ifndef VULKAN_RAYTRACE
//#warning "Missing preprocessor define VULKAN_RAYTRACE"
#endif

namespace core_internal::rendering::raytracing {

// Internal tools for RaytraceBuilder
namespace tools {
struct AccelerationStructureBuildData {
  VkAccelerationStructureTypeKHR asType =
      VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;

  std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo;

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

  VkAccelerationStructureCreateInfoKHR makeCreateInfo() {
    assert(asType != VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR &&
           "Acceleration Structure Type not set");
    assert(sizeInfo.accelerationStructureSize > 0 &&
           "Acceleration Structure Size not set");

    VkAccelerationStructureCreateInfoKHR createInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = asType;
    createInfo.size = sizeInfo.accelerationStructureSize;

    return createInfo;
  }

  VkAccelerationStructureBuildSizesInfoKHR finalizeGeometry(
      VulkanDevice* device, VkBuildAccelerationStructureFlagsKHR flags) {
    assert(asGeometry.size() > 0 && "No geometry added to Build Structure");
    assert(asType != VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR &&
           "Acceleration Structure Type not set");

    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = asType;
    buildInfo.flags = flags;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.geometryCount = static_cast<uint32_t>(asGeometry.size());
    buildInfo.pGeometries = asGeometry.data();
    buildInfo.ppGeometries = nullptr;
    buildInfo.scratchData.deviceAddress = 0;

    std::vector<uint32_t> maxPrimCount(asBuildRangeInfo.size());
    for (size_t i = 0; i < asBuildRangeInfo.size(); ++i) {
      maxPrimCount[i] = asBuildRangeInfo[i].primitiveCount;
    }

    device->getExt().pfnGetAccelerationStructureBuildSizesKHR(
        device->operator VkDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
        maxPrimCount.data(), &sizeInfo);

    return sizeInfo;
  }
};

struct AccelData {
  VkAccelerationStructureKHR accel;
  core_internal::rendering::Buffer buf;
};

struct ScratchSizeInfo {
  VkDeviceSize maxScratch;
  VkDeviceSize totalScratch;
};

class BlasBuilder {
 protected:
  VulkanDevice* device;
  VkQueryPool queryPool;

  uint32_t currentBlasIdx = 0;
  uint32_t currentQueryIdx = 0;

  std::vector<AccelData> cleanupBlasAccel;

  struct Stats {
    VkDeviceSize totalOriginalSize = 0;
    VkDeviceSize totalCompactSize = 0;

    std::string toString() const {
      return "Total Original Size: " + totalOriginalSize +
             *". Total Compact Size" + totalCompactSize;
    }
  } stats;

 public:
  BlasBuilder(core_internal::rendering::VulkanDevice* device)
      : device(device) {}
  ~BlasBuilder() {}

  std::string getStatistics() { return stats.toString(); }

  ScratchSizeInfo calculateScratchAlignedSizes(
      const std::vector<AccelerationStructureBuildData>& buildData,
      uint32_t minAlignment) {
    VkDeviceSize maxScratch{0};
    VkDeviceSize totalScratch{0};

    for (auto& buildInfo : buildData) {
      VkDeviceSize alignedSize =
          alignUp(buildInfo.sizeInfo.buildScratchSize, minAlignment);
      maxScratch = std::max(maxScratch, alignedSize);
      totalScratch += alignedSize;
    }

    return {maxScratch, totalScratch};
  }

  VkDeviceSize getScratchSize(
      VkDeviceSize hintMaxSize,
      std::vector<AccelerationStructureBuildData>& buildData,
      uint32_t minAlignment) {
    ScratchSizeInfo sizeInfo =
        calculateScratchAlignedSizes(buildData, minAlignment);
    VkDeviceSize maxScratch = sizeInfo.maxScratch;
    VkDeviceSize totalScratch = sizeInfo.totalScratch;

    if (totalScratch < hintMaxSize) {
      return totalScratch;
    } else {
      uint64_t numScratch = std::max(uint64_t(1), hintMaxSize / maxScratch);
      numScratch = std::min(numScratch, buildData.size());
      return numScratch * maxScratch;
    }
  }

  void getScratchAddresses(
      VkDeviceSize hintMaxBudget,
      const std::vector<AccelerationStructureBuildData>& buildData,
      VkDeviceAddress scratchBufferAddress,
      std::vector<VkDeviceAddress>& scratchAddresses, uint32_t minAlignment) {
    ScratchSizeInfo sizeInfo =
        calculateScratchAlignedSizes(buildData, minAlignment);
    VkDeviceSize maxScratch = sizeInfo.maxScratch;
    VkDeviceSize totalScratch = sizeInfo.totalScratch;

    // Strategy 1: scratch was large enough for all BLAS, return the addresses
    // in order
    if (totalScratch < hintMaxBudget) {
      VkDeviceAddress address = {};
      for (auto& buildInfo : buildData) {
        scratchAddresses.push_back(scratchBufferAddress + address);
        VkDeviceSize alignedSize =
            alignUp(buildInfo.sizeInfo.buildScratchSize, minAlignment);
        address += alignedSize;
      }
    }
    // Strategy 2: there are n-times the max scratch fitting in the budget
    else {
      // Make sure there is at least one scratch buffer, and not more than the
      // number of BLAS
      uint64_t numScratch = std::max(uint64_t(1), hintMaxBudget / maxScratch);
      numScratch = std::min(numScratch, buildData.size());

      VkDeviceAddress address = {};
      for (int i = 0; i < numScratch; i++) {
        scratchAddresses.push_back(scratchBufferAddress + address);
        address += maxScratch;
      }
    }
  }

  bool cmdCreateParallelBlas(
      VkCommandBuffer cmd,
      std::vector<AccelerationStructureBuildData>& blasBuildData,
      std::vector<AccelData> blasAccel,
      std::vector<VkDeviceAddress>& scratchAddresses,
      VkDeviceSize hintMaxBudget) {
    initializeQueryPool(blasBuildData);

    VkDeviceSize processBudget = 0;
    uint32_t currentQueryIdx = currentQueryIdx;
    // Process each BLAS in the data vector while staying under the memory
    // budget.
    while (currentBlasIdx < blasBuildData.size() &&
           processBudget < hintMaxBudget) {
      // Build acceleration structures and accumulate the total memory used.
      processBudget += buildAccelerationStructures(
          cmd, blasBuildData, blasAccel, scratchAddresses, hintMaxBudget,
          processBudget, currentQueryIdx);
    }

    // Check if all BLAS have been built.
    return currentBlasIdx >= blasBuildData.size();
  }

  void initializeQueryPool(
      const std::vector<AccelerationStructureBuildData>& blasBuildData) {
    if (!queryPool) {
      // Iterate through each BLAS build data element to check if the compaction
      // flag is set.
      for (const auto& blas : blasBuildData) {
        if (blas.buildInfo.flags &
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) {
          VkQueryPoolCreateInfo qpCI{
              .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
              .queryType =
                  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
              .queryCount = static_cast<uint32_t>(blasBuildData.size()),
          };
          vkCreateQueryPool(device->operator VkDevice(), &qpCI, nullptr,
                            &queryPool);
          break;
        }
      }
    }

    // If a query pool is now available (either newly created or previously
    // existing), reset the query pool to clear any old data or states.
    if (queryPool) {
      vkResetQueryPool(device->operator VkDevice(), queryPool, 0,
                       static_cast<uint32_t>(blasBuildData.size()));
    }
  }

  VkDeviceSize buildAccelerationStructures(
      VkCommandBuffer cmd,
      std::vector<AccelerationStructureBuildData>& blasBuildData,
      std::vector<AccelData>& blasAccel,
      const std::vector<VkDeviceAddress>& scratchAddress,
      VkDeviceSize hintMaxBudget, VkDeviceSize currentBudget,
      uint32_t& currentQueryIdx) {
    // Temporary vectors for storing build-related data
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> collectedBuildInfo;
    std::vector<VkAccelerationStructureKHR> collectedAccel;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> collectedRangeInfo;

    // Pre-allocate memory based on the number of BLAS to be built
    collectedBuildInfo.reserve(blasBuildData.size());
    collectedAccel.reserve(blasBuildData.size());
    collectedRangeInfo.reserve(blasBuildData.size());

    // Initialize the total budget used in this function call
    VkDeviceSize budgetUsed = 0;

    // Loop through BLAS data while there is scratch address space and budget
    // available
    while (collectedBuildInfo.size() < scratchAddress.size() &&
           currentBudget + budgetUsed < hintMaxBudget &&
           currentBlasIdx < blasBuildData.size()) {
      auto& data = blasBuildData[currentBlasIdx];
      VkAccelerationStructureCreateInfoKHR createInfo = data.makeCreateInfo();

      // Create and store acceleration structure
      blasAccel[currentBlasIdx] = createAcceleration(createInfo);
      collectedAccel.push_back(blasAccel[currentBlasIdx].accel);

      // Setup build information for the current BLAS
      data.buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      data.buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
      data.buildInfo.dstAccelerationStructure = blasAccel[currentBlasIdx].accel;
      data.buildInfo.scratchData.deviceAddress =
          scratchAddress[currentBlasIdx % scratchAddress.size()];
      data.buildInfo.pGeometries = data.asGeometry.data();
      collectedBuildInfo.push_back(data.buildInfo);
      collectedRangeInfo.push_back(data.asBuildRangeInfo.data());

      // Update the used budget with the size of the current structure
      budgetUsed += data.sizeInfo.accelerationStructureSize;
      currentBlasIdx++;
    }

    // Command to build the acceleration structures on the GPU
    device->getExt().pfnCmdBuildAccelerationStructuresKHR(
        cmd, static_cast<uint32_t>(collectedBuildInfo.size()),
        collectedBuildInfo.data(), collectedRangeInfo.data());

    // Barrier to ensure proper synchronization after building
    accelerationStructureBarrier(cmd,
                                 VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                 VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

    // If a query pool is available, record the properties of the built
    // acceleration structures
    if (queryPool) {
      device->getExt().pfnCmdWriteAccelerationStructuresPropertiesKHR(
          cmd, static_cast<uint32_t>(collectedAccel.size()),
          collectedAccel.data(),
          VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool,
          currentQueryIdx);
      currentQueryIdx += static_cast<uint32_t>(collectedAccel.size());
    }

    // Return the total budget used in this operation
    return budgetUsed;
  }

  inline void accelerationStructureBarrier(VkCommandBuffer cmd,
                                           VkAccessFlags src,
                                           VkAccessFlags dst) {
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = src;
    barrier.dstAccessMask = dst;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
  }

  AccelData createAcceleration(const VkAccelerationStructureCreateInfoKHR& CI) {
    AccelData resultAccel;

    VkBufferCreateInfo bufCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = CI.size,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VkAccelerationStructureCreateInfoKHR accel = CI;
    accel.buffer = resultAccel.buf.buffer;

    device->getExt().pfnCreateAccelerationStructureKHR(
        device->operator VkDevice(), &accel, nullptr, &resultAccel.accel);

    VkAccelerationStructureDeviceAddressInfoKHR info{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    info.accelerationStructure = resultAccel.accel;
    resultAccel.buf.deviceAddress =
        device->getExt().pfnGetAccelerationStructureDeviceAddressKHR(
            device->operator VkDevice(), &info);

    return resultAccel;
  }

  void cmdCompactBlas(
      VkCommandBuffer cmd,
      std::vector<tools::AccelerationStructureBuildData>& buildData,
      std::vector<AccelData>& blas) {
    // Compute the number of queries that have been conducted between the
    // current BLAS index and the query index.
    uint32_t queryCtn = currentBlasIdx - currentQueryIdx;
    // Ensure there is a valid query pool and BLAS to compact;
    if (queryPool == VK_NULL_HANDLE || queryCtn == 0) {
      return;
    }

    // Retrieve the compacted sizes from the query pool.
    std::vector<VkDeviceSize> compactSizes(queryCtn);
    vkGetQueryPoolResults(device->operator VkDevice(), queryPool,
                          currentQueryIdx, (uint32_t)compactSizes.size(),
                          compactSizes.size() * sizeof(VkDeviceSize),
                          compactSizes.data(), sizeof(VkDeviceSize),
                          VK_QUERY_RESULT_WAIT_BIT);

    // Iterate through each BLAS index to process compaction.
    for (size_t i = currentQueryIdx; i < currentBlasIdx; i++) {
      size_t idx =
          i -
          currentQueryIdx;  // Calculate local index for compactSizes vector.
      VkDeviceSize compactSize = compactSizes[idx];
      if (compactSize > 0) {
        // Update statistical tracking of sizes before and after compaction.
        stats.totalCompactSize += compactSize;
        stats.totalOriginalSize +=
            buildData[i].sizeInfo.accelerationStructureSize;
        buildData[i].sizeInfo.accelerationStructureSize = compactSize;
        cleanupBlasAccel.push_back(blas[i]);  // Schedule old BLAS for cleanup.

        // Create a new acceleration structure for the compacted BLAS.
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        asCreateInfo.size = compactSize;
        asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blas[i] = createAcceleration(asCreateInfo);

        // Command to copy the original BLAS to the newly created compacted
        // version.
        VkCopyAccelerationStructureInfoKHR copyInfo{
            VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        copyInfo.src = buildData[i].buildInfo.dstAccelerationStructure;
        copyInfo.dst = blas[i].accel;
        copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        device->getExt().pfnCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

        // Update the build data to reflect the new destination of the BLAS.
        buildData[i].buildInfo.dstAccelerationStructure = blas[i].accel;
      }
    }

    // Update the query index to the current BLAS index, marking the end of
    // processing for these structures.
    currentQueryIdx = currentBlasIdx;
  }

  void destroyNonCompactedBlas() {
    for (auto& blas : cleanupBlasAccel) {
      device->destroy(&blas.buf);
    }
    cleanupBlasAccel.clear();
  }
};
}  // namespace tools

class RayTraceBuilder {
 protected:
  VulkanDevice* vulkanDevice;
  std::vector<tools::AccelData> blas;
  tools::AccelData tlas;

  PFN_vkGetAccelerationStructureDeviceAddressKHR
      pfnGetAccelerationStructureDeviceAddressKHR;

 public:
  struct BlasInput {
    std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo;
    VkBuildAccelerationStructureFlagsKHR asFlags{0};
  };

  explicit RayTraceBuilder(core_internal::rendering::VulkanDevice*);
  ~RayTraceBuilder();

  void buildBlas(const std::vector<BlasInput>& input,
                 VkBuildAccelerationStructureFlagsKHR flags =
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
  void buildTlas(
      const std::vector<VkAccelerationStructureInstanceKHR>& instances,
      VkBuildAccelerationStructureFlagsKHR flags =
          VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      bool update = false);

  uint64_t getBlasDeviceAddress(uint32_t blasId);

  VkAccelerationStructureKHR getAccelerationStructure();
};
}  // namespace core_internal::rendering::raytracing