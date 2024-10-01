# CMakeList.txt : CMake project for VulkanPathTracer, include source and define
# project specific logic here.
cmake_minimum_required(VERSION 3.11)

set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.3.261.1")
find_package(Vulkan)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h" "src/*.hpp" "src/**/*.cpp" "src/**/*.h" "src/**/*.hpp" "src/***/*.cpp" "src/***/*.h" "src/***/*.hpp" )

# Add source to this project's executable.
add_executable (VulkanPathTracer ${SOURCES} ${include_files})

target_include_directories(VulkanPathTracer PRIVATE source_dir, include_dir)
target_include_directories(VulkanPathTracer PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(VulkanPathTracer PRIVATE ${Ktx_LIBRARY})
target_link_libraries(VulkanPathTracer PUBLIC ${Vulkan_LIBRARIES} glfw)
target_link_libraries(VulkanPathTracer PRIVATE imgui::imgui)
target_link_libraries(VulkanPathTracer PRIVATE gli)
target_link_libraries(VulkanPathTracer PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanPathTracer PROPERTY CXX_STANDARD 20)
endif()