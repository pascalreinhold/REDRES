#pragma once

#include "vulkan_types.hpp"
#include "buffer.hpp"
#include "mesh.hpp"

#include <glm/glm.hpp>
#include <deque>

#ifndef RCC_POINT_LIGHT_COUNT
#define RCC_POINT_LIGHT_COUNT 1
#endif
#ifndef RCC_MOUSE_BUCKET_COUNT
#define RCC_MOUSE_BUCKET_COUNT 4096
#endif

#define RCC_MESH_COUNT 5

namespace rcc {

struct PointLight {
  glm::vec4 position{5.f, 5.f, 5.f, 1.f};
  glm::vec4 lightColor{1.f};
};

struct GPUInstance {
  uint32_t object_id; // maps to unique object
  uint32_t batch_id;  // will map to a specific draw call later on
};

struct GPUOffsets {
  glm::vec4 offsets[27];
};

struct GPUFinalInstance {
  uint32_t object_id;
  uint32_t offset_id;
};

struct GPUDrawCalls {
  vk::DrawIndexedIndirectCommand commands[RCC_MESH_COUNT];
};

struct GPUObjectData {
  glm::mat4 modelMatrix;
  glm::vec4 color1;
  glm::vec4 color2;
  glm::vec4 bond_normal;
  float radius;
  uint32_t batchID;
  uint32_t padding2;
  uint32_t padding3;
};

struct GPUCullData {
  glm::mat4 viewMatrix;
  glm::vec4 frustumNormalEquations[6];
  glm::vec4 cylinderCenter;
  glm::vec4 cylinderNormal;
  float cylinderLength;
  float cylinderRadiusSquared;
  uint32_t uniqueObjectCount;
  uint32_t offsetCount;
  alignas(4) bool isCullingEnabled;
  alignas(4) bool cullCylinder;
};

struct GPUCamData {
  glm::mat4 projViewMat;
  glm::mat4 viewMat;
  glm::vec4 cam_position;
  glm::vec4 direction_of_light;
};

struct GPUSceneData {
  glm::vec4 ambientColor;
  glm::vec4 params[RCC_MESH_COUNT];//RECIPROCALGAMMA_SHININESS_DIFFUSECOEFF_SPECULAR_COEFF;
  glm::vec4 mouseCoords;
  PointLight pointLights[RCC_POINT_LIGHT_COUNT];
};

struct FrameData {
  BufferResource cam_buffer{};
  BufferResource object_buffer{};
  BufferResource cull_data_buffer{};
  BufferResource instance_buffer{};
  BufferResource final_instance_buffer{};
  BufferResource offset_buffer{};
  BufferResource draw_call_buffer{};
  BufferResource mouseBucketBuffer{};

  vk::DescriptorSet globalDescriptorSet, test_compute_shader_set;
  vk::Semaphore present_semaphore, render_semaphore;
  vk::Fence render_fence;
  vk::CommandPool command_pool;
  vk::CommandBuffer main_command_buffer;
};

struct SpecializationConstants {
  const uint32_t point_light_count = RCC_POINT_LIGHT_COUNT;
  const uint32_t mouse_bucket_count = RCC_MOUSE_BUCKET_COUNT;
};

struct Texture {
  AllocatedImage image = {};
  vk::ImageView imageView = {};
};

template<typename T>
struct Averager {
  int max_elements = 20;
  std::deque<T> q;

  void feed(T val) {
    q.push_back(val);
    if (q.size() > max_elements) { q.pop_front(); }
  }

  T avg() {
    if (q.empty()) return 0;

    T sum = 0;
    for (const T val : q) {
      sum += val;
    }
    return sum/q.size();
  }
};

vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent);
vk::ImageViewCreateInfo imageviewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);
vk::SamplerCreateInfo samplerCreateInfo(vk::Filter filters,
                                        vk::SamplerAddressMode samplerAddressMode = vk::SamplerAddressMode::eRepeat);

namespace xyz_reader {
struct symbol_string { char str[4]; };
using structureFrameData = std::tuple<std::vector<symbol_string>, std::vector<glm::vec3>, glm::mat3>;

glm::mat3 getBasisFromString(const char *text);
void readFile(const std::string &filename, std::vector<structureFrameData> &data);
void printStructureData(int frameCount, const std::vector<structureFrameData> &data);
}

}