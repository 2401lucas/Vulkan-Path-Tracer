#pragma once

#include <vulkan/vulkan.h>

#include <cassert>
#include <iostream>
#include <string>

namespace core_internal::rendering {
#define DEBUG_ERROR(f)                                             \
  {                                                                \
    std::string res = (f);                                         \
    std::cerr << "Fatal Error : \"" << res << "\" in " << __FILE__ \
              << " at line " << __LINE__ << "\n";                  \
    exit(2401);                                                    \
  }
#define DEBUG_WARNING(f)                                                      \
  {                                                                           \
    std::string res = (f);                                                    \
    std::cout << "Warning : \"" << res << "\" in " << __FILE__ << " at line " \
              << __LINE__ << "\n";                                            \
  }
#define DEBUG_LOG(f)       \
  {                        \
    std::string res = (f); \
    std::cout << res;      \
  }

#define VK_CHECK_RESULT(f)                                                \
  {                                                                       \
    VkResult res = (f);                                                   \
    if (res != VK_SUCCESS) {                                              \
      std::cout << "Fatal : VkResult is \""                               \
                << core_internal::rendering::errorString(res) << "\" in " \
                << __FILE__ << " at line " << __LINE__ << "\n";           \
      assert(res == VK_SUCCESS);                                          \
    }                                                                     \
  }

static std::string errorString(VkResult errorCode) {
  switch (errorCode) {
#define STR(r) \
  case VK_##r: \
    return #r
    STR(NOT_READY);
    STR(TIMEOUT);
    STR(EVENT_SET);
    STR(EVENT_RESET);
    STR(INCOMPLETE);
    STR(ERROR_OUT_OF_HOST_MEMORY);
    STR(ERROR_OUT_OF_DEVICE_MEMORY);
    STR(ERROR_INITIALIZATION_FAILED);
    STR(ERROR_DEVICE_LOST);
    STR(ERROR_MEMORY_MAP_FAILED);
    STR(ERROR_LAYER_NOT_PRESENT);
    STR(ERROR_EXTENSION_NOT_PRESENT);
    STR(ERROR_FEATURE_NOT_PRESENT);
    STR(ERROR_INCOMPATIBLE_DRIVER);
    STR(ERROR_TOO_MANY_OBJECTS);
    STR(ERROR_FORMAT_NOT_SUPPORTED);
    STR(ERROR_SURFACE_LOST_KHR);
    STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    STR(SUBOPTIMAL_KHR);
    STR(ERROR_OUT_OF_DATE_KHR);
    STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    STR(ERROR_VALIDATION_FAILED_EXT);
    STR(ERROR_INVALID_SHADER_NV);
    STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
    default:
      return "UNKNOWN_ERROR";
  }
}

static std::string physicalDeviceTypeString(VkPhysicalDeviceType type) {
  switch (type) {
#define STR(r)                      \
  case VK_PHYSICAL_DEVICE_TYPE_##r: \
    return #r
    STR(OTHER);
    STR(INTEGRATED_GPU);
    STR(DISCRETE_GPU);
    STR(VIRTUAL_GPU);
    STR(CPU);
#undef STR
    default:
      return "UNKNOWN_DEVICE_TYPE";
  }
}

// TODO MOVE
template <class integral>
constexpr bool isAligned(integral x, size_t a) noexcept {
  return (x & (integral(a) - 1)) == 0;
}

template <class integral>
constexpr integral alignUp(integral x, size_t a) noexcept {
  return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

template <class integral>
constexpr integral alignDown(integral x, size_t a) noexcept {
  return integral(x & ~integral(a - 1));
}

}  // namespace core_internal::rendering