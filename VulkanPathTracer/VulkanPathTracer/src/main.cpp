#include <array>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define VULKAN_DEBUG_EXT
#define VULKAN_RAYTRACE

#include "Core/Tools/HelperMacros.hpp"
#include "Core/Vulkan/VulkanDescriptorSet.hpp"
#include "Core/Vulkan/VulkanDevice.h"
#include "VulkanResources/RayTraceHelper.hpp"

// TODO: USE IMGUI TO SHOW/GENERATE MORE IMAGES
static const uint32_t RenderWidth = 800;
static const uint32_t RenderHeight = 600;
static const uint32_t WorkgroupWidth = 16;
static const uint32_t WorkgroupHeight = 8;

int main(int argc, const char** argv) {
  std::vector<const char*> deviceExtensions;
  std::vector<const char*> instanceExtensions;

  deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
  deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

  core_internal::rendering::VulkanDevice* device =
      new core_internal::rendering::VulkanDevice(
          "PathTracer", false, deviceExtensions, instanceExtensions, nullptr,
          VK_API_VERSION_1_3);

  VkBufferCreateInfo bufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = RenderWidth * RenderHeight * 3 * sizeof(float),
      .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  core_internal::rendering::Buffer* buf =
      new core_internal::rendering::Buffer();
  device->createBuffer(buf, bufferInfo,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  std::vector<core_internal::rendering::raytracing::RayTraceBuilder::BlasInput>
      blases;

  core_internal::rendering::Buffer* vertexBuffer;
  core_internal::rendering::Buffer* indexBuffer;

  // For Each static model
  // Load model
  tinyobj::ObjReader reader;  // Used to read an OBJ file
  reader.ParseFromFile("assets/CornellBox-Original-Merged.obj");

  const std::vector<tinyobj::real_t> objVertices =
      reader.GetAttrib().GetVertices();
  const std::vector<tinyobj::shape_t>& objShapes =
      reader.GetShapes();         // All shapes in the file
  assert(objShapes.size() == 1);  // Check that this file has only one shape
  const tinyobj::shape_t& objShape = objShapes[0];  // Get the first shape
  // Get the indices of the vertices of the first mesh of `objShape` in
  // `attrib.vertices`:
  std::vector<uint32_t> objIndices;
  objIndices.reserve(objShape.mesh.indices.size());
  for (const tinyobj::index_t& index : objShape.mesh.indices) {
    objIndices.push_back(index.vertex_index);
  }

  VkBufferCreateInfo vertBufCI{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = objVertices.size() * sizeof(tinyobj::real_t),
      .usage =
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
  };

  VkBufferCreateInfo indBufCI{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = objIndices.size() * sizeof(uint32_t),
      .usage =
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
  };

  device->createBuffer(vertexBuffer, vertBufCI,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  device->createBuffer(indexBuffer, indBufCI,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  device->copyMemoryToAlloc(vertexBuffer, (void*)objVertices.data(),
                            vertBufCI.size);
  device->copyMemoryToAlloc(indexBuffer, (void*)objIndices.data(),
                            indBufCI.size);

  core_internal::rendering::raytracing::RayTraceBuilder::BlasInput blas;
  VkAccelerationStructureGeometryTrianglesDataKHR triangles{
      .sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData{.deviceAddress = vertexBuffer->deviceAddress},
      .vertexStride = 3 * sizeof(float),
      .maxVertex = static_cast<uint32_t>(objVertices.size() / 3 - 1),
      .indexType = VK_INDEX_TYPE_UINT32,
      .indexData{.deviceAddress = indexBuffer->deviceAddress},
      .transformData{.deviceAddress = 0},  // No transform
  };

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
      .geometry{
          .triangles = triangles,
      },
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };

  blas.asGeometry.push_back(geometry);

  VkAccelerationStructureBuildRangeInfoKHR offsetInfo{
      .primitiveCount =
          static_cast<uint32_t>(objIndices.size() / 3),  // Number of triangles
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  blas.asBuildRangeInfo.push_back(offsetInfo);
  blases.push_back(blas);

  auto rtBuilder =
      new core_internal::rendering::raytracing::RayTraceBuilder(device);

  // Builds Static BLAS
  rtBuilder->buildBlas(
      blases, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

  std::vector<VkAccelerationStructureInstanceKHR> instances;
  {
    VkAccelerationStructureInstanceKHR instance{};
    instance.accelerationStructureReference =
        rtBuilder->getBlasDeviceAddress(0);
    // The address of the BLAS in `blases` that this instance
    // points to
    // Set the instance transform to the identity matrix:
    instance.transform.matrix[0][0] = instance.transform.matrix[1][1] =
        instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex =
        0;  // 24 bits accessible to ray shaders via
            // rayQueryGetIntersectionInstanceCustomIndexEXT
    // Used for a shader offset index, accessible via
    // rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags =
        VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // How to
                                                                    // trace
                                                                    // this
                                                                    // instance
    instance.mask = 0xFF;
    instances.push_back(instance);
  }

  rtBuilder->buildTlas(
      instances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      false);

  core_internal::rendering::VulkanDescriptorSet* descriptorSet =
      new core_internal::rendering::VulkanDescriptorSet(device);

  descriptorSet->addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                            VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSet->addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
  descriptorSet->addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                            VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSet->addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                            VK_SHADER_STAGE_COMPUTE_BIT);
  descriptorSet->initLayout();
  descriptorSet->initPool(1);
  descriptorSet->initPipelineLayout();

  std::array<VkWriteDescriptorSet, 4> writeDescriptorSets;

  VkDescriptorBufferInfo descriptorBufferInfo{
      .buffer = buf->buffer,
      .range = buf->size,
  };
  writeDescriptorSets[0] =
      descriptorSet->makeWrite(0, 0, &descriptorBufferInfo);

  VkAccelerationStructureKHR tlasCopy = rtBuilder->getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descriptorAS{
      .sType =
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
      .accelerationStructureCount = 1,
      .pAccelerationStructures = &tlasCopy,
  };
  writeDescriptorSets[1] = descriptorSet->makeWrite(0, 1, &descriptorAS);

  VkDescriptorBufferInfo vertexDescriptorBufferInfo{
      .buffer = vertexBuffer->buffer,
      .range = vertexBuffer->size,
  };
  writeDescriptorSets[2] =
      descriptorSet->makeWrite(0, 2, &vertexDescriptorBufferInfo);

  VkDescriptorBufferInfo indexDescriptorBufferInfo{
      .buffer = indexBuffer->buffer,
      .range = indexBuffer->size,
  };
  writeDescriptorSets[3] =
      descriptorSet->makeWrite(0, 3, &indexDescriptorBufferInfo);

  vkUpdateDescriptorSets(device->operator VkDevice(),
                         static_cast<uint32_t>(writeDescriptorSets.size()),
                         writeDescriptorSets.data(), 0, nullptr);

  VkPipelineShaderStageCreateInfo rayTraceStage =
      device->loadShader("shaders/pt.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  VkComputePipelineCreateInfo pipelineCI{
      .stage = rayTraceStage,
      .layout = descriptorSet->operator VkPipelineLayout(),
  };

  VkPipeline computePipeline;
  VK_CHECK_RESULT(vkCreateComputePipelines(device->operator VkDevice(),
                                           VK_NULL_HANDLE, 1, &pipelineCI,
                                           nullptr, &computePipeline));

  VkCommandBuffer cmdBuffer = device->createCommandBuffer();

  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

  auto ds = descriptorSet->getSet(0);
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          descriptorSet->operator VkPipelineLayout(), 0, 1, &ds,
                          0, nullptr);

  vkCmdDispatch(
      cmdBuffer, (uint32_t(RenderWidth) + WorkgroupWidth - 1) / WorkgroupWidth,
      (uint32_t(RenderHeight) + WorkgroupHeight - 1) / WorkgroupHeight, 1);

  VkMemoryBarrier memoryBarrier{
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
  };

  vkCmdPipelineBarrier(
      cmdBuffer,                             // The command buffer
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // From the compute shader
      VK_PIPELINE_STAGE_HOST_BIT,            // To the CPU
      0,                                     // No special flags
      1, &memoryBarrier,                     // An array of memory barriers
      0, nullptr, 0, nullptr);               // No other barriers

  vkEndCommandBuffer(cmdBuffer);
  device->submitCommandBuffer(cmdBuffer);
  device->waitIdle();

  char* data = new char[buf->size];
  device->copyAllocToMemory(buf, data);

  stbi_write_hdr("out.hdr", RenderWidth, RenderHeight, 3,
                 reinterpret_cast<float*>(data));

  vkDestroyPipeline(device->operator VkDevice(), computePipeline, nullptr);
  delete descriptorSet;
  delete rtBuilder;
  device->destroy(vertexBuffer);
  device->destroy(indexBuffer);
  device->destroy(buf);
  delete device;
}