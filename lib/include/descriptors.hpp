//
// Created by x on 11/24/22.
//

#pragma once
#include "buffer.hpp"

#include <vulkan/vulkan.hpp>


#include <vector>
#include <unordered_map>

namespace rcc {

class DescriptorAllocator {
 public:
  struct PoolSizes {
    std::vector<std::pair<vk::DescriptorType, float>> data = {
        {vk::DescriptorType::eSampler, 0.5f},
        {vk::DescriptorType::eCombinedImageSampler, 4.f},
        {vk::DescriptorType::eSampledImage, 4.f},
        {vk::DescriptorType::eStorageImage, 1.f},
        {vk::DescriptorType::eUniformTexelBuffer, 1.f},
        {vk::DescriptorType::eStorageTexelBuffer, 1.f},
        {vk::DescriptorType::eUniformBuffer, 2.f},
        {vk::DescriptorType::eStorageBuffer, 2.f},
        {vk::DescriptorType::eUniformBufferDynamic, 1.f},
        {vk::DescriptorType::eStorageBufferDynamic, 1.f},
        {vk::DescriptorType::eInputAttachment, 0.5f}
    };
  };

  void reset_pools();
  bool allocate(vk::DescriptorSet *set, vk::DescriptorSetLayout layout);
  void cleanup();
  void init(vk::Device device);

  vk::Device logical_device;
 private:

  vk::DescriptorPool grab_pool();
  vk::DescriptorPool currentPool{nullptr};
  PoolSizes descriptorSizes;
  std::vector<vk::DescriptorPool> usedPools;
  std::vector<vk::DescriptorPool> freePools;
};

class DescriptorLayoutCache {
 public:
  void init(vk::Device device);
  void cleanup();
  vk::DescriptorSetLayout createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo *info);

  struct DescriptorLayoutInfo {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bool operator==(const DescriptorLayoutInfo &rhs) const;
    size_t hash() const;
  };

 private:
  struct DescriptorLayoutHash {
    size_t operator()(const DescriptorLayoutInfo &layout_info) const {
      return layout_info.hash();
    }
  };

  std::unordered_map<DescriptorLayoutInfo, vk::DescriptorSetLayout, DescriptorLayoutHash> layout_cache;
  vk::Device logical_device;
};

class DescriptorBuilder {
 public:
  static DescriptorBuilder begin(DescriptorLayoutCache *layout_cache, DescriptorAllocator *allocator);

  DescriptorBuilder &bindBuffer(uint32_t binding,
                                const BufferResource& buffer_resource,
                                vk::ShaderStageFlags stageFlags);
  DescriptorBuilder &bindImage(uint32_t binding,
                               vk::DescriptorImageInfo *imageInfo,
                               vk::DescriptorType type,
                               vk::ShaderStageFlags stageFlags);

  bool build(vk::DescriptorSet &set, vk::DescriptorSetLayout &layout);
  bool build(vk::DescriptorSet &set);
 private:

  std::vector<vk::WriteDescriptorSet> writes;
  std::vector<vk::DescriptorSetLayoutBinding> bindings;

  DescriptorLayoutCache *cache;
  DescriptorAllocator *alloc;
};

}
