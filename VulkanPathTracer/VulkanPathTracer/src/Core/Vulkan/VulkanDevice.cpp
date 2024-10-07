#define VMA_IMPLEMENTATION
#include "VulkanDevice.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "../Tools/HelperMacros.hpp"

namespace core_internal::rendering {
VulkanDevice::VulkanDevice(const char *name, bool useValidation,
                           std::vector<const char *> enabledDeviceExtensions,
                           std::vector<const char *> enabledInstanceExtensions,
                           void *pNextChain, uint32_t requestedVulkanAPI) {
  apiVersion = requestedVulkanAPI;
  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = name,
      .pEngineName = "Blu",
      .apiVersion = apiVersion,
  };

  std::vector<const char *> instanceExtensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

  // Get extensions supported by the instance and store for later use
  uint32_t extCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
  if (extCount > 0) {
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkEnumerateInstanceExtensionProperties(
            nullptr, &extCount, &extensions.front()) == VK_SUCCESS) {
      for (VkExtensionProperties &extension : extensions) {
        supportedInstanceExtensions.push_back(extension.extensionName);
      }
    }
  }

  // Enabled requested instance extensions
  if (enabledInstanceExtensions.size() > 0) {
    for (const char *enabledExtension : enabledInstanceExtensions) {
      // Output message if requested extension is not available
      if (std::find(supportedInstanceExtensions.begin(),
                    supportedInstanceExtensions.end(),
                    enabledExtension) == supportedInstanceExtensions.end()) {
        DEBUG_ERROR("Enabled instance extension \"" + *enabledExtension +
                    *"\" is not present at instance level\n");
      }
      instanceExtensions.push_back(enabledExtension);
    }
  }

  VkInstanceCreateInfo instanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = NULL,
      .pApplicationInfo = &appInfo,
  };

  VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI;
  if (useValidation) {
    debugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        //.pfnUserCallback = debugUtilsMessageCallback,
    };

    debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
    instanceCreateInfo.pNext = &debugUtilsMessengerCI;
  }

  if (useValidation || std::find(supportedInstanceExtensions.begin(),
                                 supportedInstanceExtensions.end(),
                                 VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
                           supportedInstanceExtensions.end()) {
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  if (instanceExtensions.size() > 0) {
    instanceCreateInfo.enabledExtensionCount =
        (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  }

  const char *validationLayerName = "VK_LAYER_KHRONOS_validation";
  if (useValidation) {
    uint32_t instanceLayerCount;
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
    vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                       instanceLayerProperties.data());
    bool validationLayerPresent = false;
    for (VkLayerProperties &layer : instanceLayerProperties) {
      if (strcmp(layer.layerName, validationLayerName) == 0) {
        validationLayerPresent = true;
        break;
      }
    }
    if (validationLayerPresent) {
      instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
      instanceCreateInfo.enabledLayerCount = 1;
    } else {
      std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, "
                   "validation is disabled";
    }
  }

  VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
  vkExt = tools::VulkanExtentions();

#ifdef VULKAN_DEBUG_EXT
  if (std::find(supportedInstanceExtensions.begin(),
                supportedInstanceExtensions.end(),
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
      supportedInstanceExtensions.end()) {
    vkExt.initDebug(instance);
  }
#endif

  // Physical device
  uint32_t gpuCount = 0;
  // Get number of available physical devices
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
  if (gpuCount == 0) {
    DEBUG_ERROR("No device with Vulkan support found");
    return;
  }

  // Enumerate devices
  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  VK_CHECK_RESULT(
      vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));

  // GPU selection
  physicalDevice = choosePhysicalDevice(physicalDevices);

  // Get list of supported extensions
  extCount = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount,
                                       nullptr);
  if (extCount > 0) {
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount,
                                             &extensions.front()) ==
        VK_SUCCESS) {
      for (auto ext : extensions) {
        supportedDeviceExtensions.push_back(ext.extensionName);
      }
    }
  }

  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);
  assert(queueFamilyCount > 0);
  queueFamilyProperties.resize(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilyProperties.data());

  // If requested, we enable the default validation layers for debugging
#ifdef VULKAN_DEBUG_EXT
  VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
      .pfnUserCallback = vkExt.vkCreateDebugUtilsMessengerEXT,

  };

  VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(
      instance, &debugUtilsMessengerCI, nullptr, &debugUtilsMessenger));
#endif

  // Desired queues need to be requested upon logical device creation
  // Due to differing queue family configurations of Vulkan implementations this
  // can be a bit tricky, especially if the application requests different queue
  // types
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

  // Get queue family indices for the requested queue family types
  // Note that the indices may overlap depending on the implementation

  const float defaultQueuePriority(0.0f);

  queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
  VkDeviceQueueCreateInfo queueInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = queueFamilyIndices.graphics,
      .queueCount = 1,
      .pQueuePriorities = &defaultQueuePriority,
  };
  queueCreateInfos.push_back(queueInfo);

  queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
  if (queueFamilyIndices.compute != queueFamilyIndices.graphics) {
    // If compute family index differs, we need an additional queue create
    // info for the compute queue
    VkDeviceQueueCreateInfo queueInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndices.compute,
        .queueCount = 1,
        .pQueuePriorities = &defaultQueuePriority,
    };
    queueCreateInfos.push_back(queueInfo);
  }

  queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
  if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) &&
      (queueFamilyIndices.transfer != queueFamilyIndices.compute)) {
    // If transfer family index differs, we need an additional queue create
    // info for the transfer queue
    VkDeviceQueueCreateInfo queueInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndices.transfer,
        .queueCount = 1,
        .pQueuePriorities = &defaultQueuePriority,
    };
    queueCreateInfos.push_back(queueInfo);
  }

  // Create the logical device representation
  std::vector<const char *> deviceExtensions(enabledDeviceExtensions);
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  VkDeviceCreateInfo deviceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = pNextChain,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .pEnabledFeatures = &enabledFeatures,
  };

  features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &deviceCreateInfo.pNext,
      .features = enabledFeatures,
  };

  deviceCreateInfo.pEnabledFeatures = nullptr;
  deviceCreateInfo.pNext = &features;

  properties = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = nullptr,
  };

#ifdef VK_RAYTRACE
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
      .pNext = &deviceCreateInfo.pNext,
  };
  deviceCreateInfo.pNext = &asFeatures;

  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
      .pNext = &deviceCreateInfo.pNext,
  };
  deviceCreateInfo.pNext = &rayQueryFeatures;

  accelProperties = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
      .pNext = nullptr,
  };
  properties.pNext = accelProperties;

#endif

  vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
  // vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

  if (deviceExtensions.size() > 0) {
    for (const char *enabledExtension : deviceExtensions) {
      if (!extensionSupported(enabledExtension)) {
        DEBUG_ERROR(
            "Enabled device extension");  //+ *enabledExtension +
                                          //"is not present at device level");
      }
    }

    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
  }

  VK_CHECK_RESULT(
      vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

  vkGetDeviceQueue(device, queueFamilyIndices.graphics, 0, &queues.graphics);
  vkGetDeviceQueue(device, queueFamilyIndices.compute, 0, &queues.compute);
  vkGetDeviceQueue(device, queueFamilyIndices.transfer, 0, &queues.transfer);

  VmaAllocatorCreateInfo vmaAllocInfo{
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = physicalDevice,
      .device = device,
      .instance = instance,
      .vulkanApiVersion = apiVersion,
  };

  vmaCreateAllocator(&vmaAllocInfo, &allocator);

  VkCommandPoolCreateInfo cmdPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndices.graphics,
  };

  VK_CHECK_RESULT(
      vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool));
}

VulkanDevice::~VulkanDevice() {
  for (auto &shaderModule : shaderModules) {
    vkDestroyShaderModule(device, shaderModule, nullptr);
  }

  if (commandPool) {
    vkDestroyCommandPool(device, commandPool, nullptr);
  }

  if (allocator) {
    vmaDestroyAllocator(allocator);
  }
  if (device) {
    vkDestroyDevice(device, nullptr);
  }
  if (instance) {
    vkDestroyInstance(instance, nullptr);
  }
}

VkPhysicalDevice VulkanDevice::choosePhysicalDevice(
    std::vector<VkPhysicalDevice> devices) {
  std::multimap<int, VkPhysicalDevice> candidates;

  for (auto &device : devices) {
    int score = rateDeviceSuitability(device);
    candidates.insert(std::make_pair(score, device));
  }

  if (candidates.rbegin()->first > 0) {
    return candidates.rbegin()->second;
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

int VulkanDevice::rateDeviceSuitability(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  // Required Features
  /*if (deviceFeatures.geometryShader != VK_TRUE) {
    return 0;
  }*/

  int score = 0;

  if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 10000;
  }
  // Maximum possible size of textures affects graphics quality
  score += deviceProperties.limits.maxImageDimension2D;

  return score;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::debugUtilsMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  // Select prefix depending on flags passed to the callback
  std::string prefix;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
#if defined(_WIN32)
    prefix = "\033[32m" + prefix + "\033[0m";
#endif
    prefix = "VERBOSE: ";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    prefix = "INFO: ";
#if defined(_WIN32)
    prefix = "\033[36m" + prefix + "\033[0m";
#endif
  } else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    prefix = "WARNING: ";
#if defined(_WIN32)
    prefix = "\033[33m" + prefix + "\033[0m";
#endif
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    prefix = "ERROR: ";
#if defined(_WIN32)
    prefix = "\033[31m" + prefix + "\033[0m";
#endif
  }

  // Display message to default output (console/logcat)
  std::stringstream debugMessage;
  debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "]["
               << pCallbackData->pMessageIdName
               << "] : " << pCallbackData->pMessage;

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    std::cerr << debugMessage.str() << "\n\n";
  } else {
    std::cout << debugMessage.str() << "\n\n";
  }
  fflush(stdout);

  // The return value of this callback controls whether the Vulkan call that
  // caused the validation message will be aborted or not We return VK_FALSE as
  // we DON'T want Vulkan calls that cause a validation message to abort If you
  // instead want to have calls abort, pass in VK_TRUE and the function will
  // return VK_ERROR_VALIDATION_FAILED_EXT
  return VK_FALSE;
}

uint32_t VulkanDevice::getMemoryType(uint32_t typeBits,
                                     VkMemoryPropertyFlags properties,
                                     VkBool32 *memTypeFound) const {
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if (typeBits & 1) {
      if ((memoryProperties.memoryTypes[i].propertyFlags & properties) ==
          properties) {
        if (memTypeFound) {
          *memTypeFound = true;
        }
        return i;
      }
    }
    typeBits >>= 1;
  }

  if (memTypeFound) {
    *memTypeFound = false;
    return 0;
  } else {
    DEBUG_ERROR("Could not find a matching memory type");
  }
}

uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const {
  // Dedicated queue for compute
  // Try to find a queue family index that supports compute but not graphics
  if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags) {
    for (uint32_t i = 0;
         i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
      if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
          ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ==
           0)) {
        return i;
      }
    }
  }
  // Dedicated queue for transfer
  // Try to find a queue family index that supports transfer but not graphics
  // and compute
  if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags) {
    for (uint32_t i = 0;
         i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
      if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
          ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ==
           0) &&
          ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
        return i;
      }
    }
  }

  // For other queue types or if no separate compute queue is present, return
  // the first one to support the requested flags
  for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size());
       i++) {
    if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
      return i;
    }
  }

  DEBUG_ERROR("Could not find a matching queue family index");
}

bool VulkanDevice::extensionSupported(std::string extension) {
  return (std::find(supportedDeviceExtensions.begin(),
                    supportedDeviceExtensions.end(),
                    extension) != supportedDeviceExtensions.end());
}

VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport) {
  // All depth formats may be optional, so we need to find a suitable depth
  // format to use
  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM};
  for (auto &format : depthFormats) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format,
                                        &formatProperties);
    // Format must support depth stencil attachment for optimal tiling
    if (formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (checkSamplingSupport) {
        if (!(formatProperties.optimalTilingFeatures &
              VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
          continue;
        }
      }
      return format;
    }
  }
  DEBUG_ERROR("Could not find a matching depth format");
}

void VulkanDevice::waitIdle() { vkDeviceWaitIdle(device); }

VkPipelineShaderStageCreateInfo VulkanDevice::loadShader(
    std::string fileName, VkShaderStageFlagBits stage) {
  auto shaderModule = loadShaderModule(fileName.c_str());
  assert(shaderModule != VK_NULL_HANDLE);

  VkPipelineShaderStageCreateInfo shaderStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = stage,
      .module = shaderModule,
      .pName = "main",
  };
  shaderModules.push_back(shaderStage.module);
  return shaderStage;
}

VkShaderModule VulkanDevice::loadShaderModule(const char *fileName) {
  std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

  if (is.is_open()) {
    size_t size = is.tellg();
    is.seekg(0, std::ios::beg);
    char *shaderCode = new char[size];
    is.read(shaderCode, size);
    is.close();

    assert(size > 0);

    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo moduleCreateInfo{};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = size;
    moduleCreateInfo.pCode = (uint32_t *)shaderCode;

    VK_CHECK_RESULT(
        vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

    delete[] shaderCode;

    return shaderModule;
  } else {
    std::cerr << "Error: Could not open shader file \"" << fileName << "\""
              << "\n";
    return VK_NULL_HANDLE;
  }
}

// Returns a command buffer that has been started
VkCommandBuffer VulkanDevice::createCommandBuffer() {
  VkCommandBufferAllocateInfo cmdBufAllocateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer cmdBuffer;
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

  VkCommandBufferBeginInfo cmdBufInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

  VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

  return cmdBuffer;
}

void VulkanDevice::submitCommandBuffer(VkCommandBuffer buf, VkFence fence) {
  VkSubmitInfo info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &buf,
  };

  vkQueueSubmit(queues.graphics, 1, &info, fence);
}

void VulkanDevice::createBuffer(Buffer *buf, const VkBufferCreateInfo &bufCI,
                                VkMemoryPropertyFlags propertyFlags,
                                VmaAllocationCreateFlags vmaFlags,
                                bool mapped) {
  uint32_t requiredFlags = 0;
  uint32_t preferredFlags = propertyFlags;
  if (mapped) {
    requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  }

  VmaAllocationCreateInfo allocCI{
      .flags = vmaFlags,
      .usage = VMA_MEMORY_USAGE_AUTO,
      .requiredFlags = requiredFlags,
      .preferredFlags = preferredFlags,
  };

  VmaAllocationInfo allocInfo;

  vmaCreateBuffer(allocator, &bufCI, &allocCI, &buf->buffer, &buf->alloc,
                  &allocInfo);

  buf->size = bufCI.size;
  buf->mappedData = allocInfo.pMappedData;

  if (bufCI.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buf->buffer};

    buf->deviceAddress = vkGetBufferDeviceAddress(device, &info);
  }
}

void VulkanDevice::copyAllocToMemory(core_internal::rendering::Buffer *buf,
                                     void *dst) {
  vmaCopyAllocationToMemory(allocator, buf->alloc, 0, dst, buf->size);
}

void VulkanDevice::copyMemoryToAlloc(core_internal::rendering::Buffer *buf,
                                     void *src, VkDeviceSize size) {
  vmaCopyMemoryToAllocation(allocator, src, buf->alloc, 0, size);
}

void VulkanDevice::destroy(Buffer *buf) {
  vmaDestroyBuffer(allocator, buf->buffer, buf->alloc);
}
};  // namespace core_internal::rendering