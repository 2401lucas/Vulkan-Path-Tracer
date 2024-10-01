#pragma once
#include <vulkan/vulkan.h>
// Defines
// VULKAN_DEBUG_EXT
// VULKAN_RAYTRACE

namespace core_internal::rendering::tools {
class VulkanExtentions {
 public:
  // VULKAN_DEBUG_EXT
  PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
  PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

  // VK_RAYTRACE
  PFN_vkGetAccelerationStructureDeviceAddressKHR
      pfnGetAccelerationStructureDeviceAddressKHR;
  PFN_vkGetAccelerationStructureBuildSizesKHR
      pfnGetAccelerationStructureBuildSizesKHR;
  PFN_vkCopyAccelerationStructureToMemoryKHR
      pfnCopyAccelerationStructureToMemoryKHR;
  PFN_vkCreateAccelerationStructureKHR pfnCreateAccelerationStructureKHR;
  PFN_vkCmdBuildAccelerationStructuresKHR pfnCmdBuildAccelerationStructuresKHR;
  PFN_vkBuildAccelerationStructuresKHR pfnBuildAccelerationStructuresKHR;
  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR
      pfnCmdWriteAccelerationStructuresPropertiesKHR;
  PFN_vkCmdCopyAccelerationStructureKHR pfnCmdCopyAccelerationStructureKHR;

  void init(VkInstance instance) {
#ifdef VULKAN_DEBUG_EXT
    vkCmdBeginDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    vkCmdEndDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    vkCmdInsertDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
    vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
#endif
  }

  void init(VkDevice device) {
#ifdef VULKAN_RAYTRACE
    pfnGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(device,
                                "vkGetAccelerationStructureBuildSizesKHR"));
    pfnGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(device,
                                "vkGetAccelerationStructureBuildSizesKHR"));
    pfnCopyAccelerationStructureToMemoryKHR =
        reinterpret_cast<PFN_vkCopyAccelerationStructureToMemoryKHR>(
            vkGetDeviceProcAddr(device,
                                "vkCopyAccelerationStructureToMemoryKHR"));
    pfnCreateAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    pfnGetAccelerationStructureDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(device,
                                "vkGetAccelerationStructureDeviceAddressKHR"));
    pfnCmdBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    pfnBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
    pfnCmdWriteAccelerationStructuresPropertiesKHR =
        reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
            vkGetDeviceProcAddr(
                device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    pfnCmdCopyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR"));
#endif
  }
};
}  // namespace core_internal::rendering::tools