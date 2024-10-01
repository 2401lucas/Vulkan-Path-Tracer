#pragma once
#define VK_USE_PLATFORM_WIN32_KHR

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "../Tools/VulkanExtentions.hpp"

namespace core_internal::rendering {
using DeviceAddress = uint64_t;

struct Buffer {
  VkBuffer buffer;
  VmaAllocation alloc;
  VkDeviceSize size;
  DeviceAddress deviceAddress;
  void *mappedData;

  // void *mapped;
  // VkDescriptorBufferInfo descriptor;
  // VkBufferUsageFlags usageFlags;
  // VkMemoryPropertyFlags memoryPropertyFlags;
};

struct Image {
  VkImage image;
  VkImageView view;
  VkSampler Sampler;
};

class VulkanDevice {
 private:
  VkInstance instance;
  uint32_t apiVersion;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  VmaAllocator allocator;
  VkPhysicalDeviceProperties2 properties;
  VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProperties;
  VkPhysicalDeviceFeatures2 features;
  VkPhysicalDeviceFeatures enabledFeatures;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  std::vector<std::string> supportedDeviceExtensions;
  std::vector<std::string> supportedInstanceExtensions;
  std::vector<VkShaderModule> shaderModules;
  // Command pool for helper functions
  VkCommandPool commandPool = VK_NULL_HANDLE;

  tools::VulkanExtentions vkExt;

#ifdef VULKAN_DEBUG_EXT
  VkDebugUtilsMessengerEXT debugUtilsMessenger{nullptr};
#endif

  struct {
    uint32_t graphics;
    uint32_t compute;
    uint32_t transfer;
  } queueFamilyIndices;

  struct {
    VkQueue graphics{VK_NULL_HANDLE};
    VkQueue compute{VK_NULL_HANDLE};
    VkQueue transfer{VK_NULL_HANDLE};
  } queues;

  VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
      void *pUserData);

 public:
  operator VkInstance() const { return instance; };
  operator VkDevice() const { return device; };
  operator VkPhysicalDevice() const { return physicalDevice; };
  operator VmaAllocator() const { return allocator; };
  operator VkPhysicalDeviceProperties() const { return properties.properties; };
  operator VkPhysicalDeviceMemoryProperties() const {
    return memoryProperties;
  };
  operator VkPhysicalDeviceAccelerationStructurePropertiesKHR() const {
    return accelProperties;
  };

  explicit VulkanDevice(const char *name, bool useValidation,
                        std::vector<const char *> enabledDeviceExtensions,
                        std::vector<const char *> enabledInstanceExtensions,
                        void *pNextChain = nullptr,
                        uint32_t requestedVulkanAPI = VK_API_VERSION_1_0);
  ~VulkanDevice();

  tools::VulkanExtentions getExt() const { return vkExt; }

  int rateDeviceSuitability(VkPhysicalDevice device);
  VkPhysicalDevice choosePhysicalDevice(std::vector<VkPhysicalDevice> devices);
  uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                         VkBool32 *memTypeFound = nullptr) const;
  uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;
  bool extensionSupported(std::string extension);
  VkFormat getSupportedDepthFormat(bool checkSamplingSupport);

  void waitIdle();

  VkPipelineShaderStageCreateInfo loadShader(std::string fileName,
                                             VkShaderStageFlagBits stage);
  VkShaderModule loadShaderModule(const char *fileName);

  // Vk Resources
  VkCommandBuffer createCommandBuffer();
  void submitCommandBuffer(VkCommandBuffer, VkFence = nullptr);

  void createBuffer(core_internal::rendering::Buffer *buf,
                    const VkBufferCreateInfo &bufCI,
                    VkMemoryPropertyFlags propertyFlags,
                    VmaAllocationCreateFlags vmaFlags = 0, bool mapped = false);

  void copyAllocToMemory(core_internal::rendering::Buffer *, void *dst);
  void copyMemoryToAlloc(core_internal::rendering::Buffer *, void *src,
                         VkDeviceSize size);

  void destroy(core_internal::rendering::Buffer *);
};
}  // namespace core_internal::rendering