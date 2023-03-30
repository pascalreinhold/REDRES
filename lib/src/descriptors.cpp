//
// Created by x on 11/24/22.
//

#include "descriptors.hpp"
#include <algorithm>

namespace rcc {

void DescriptorAllocator::reset_pools() {
  for (auto pool : usedPools) {
    logical_device.resetDescriptorPool(pool, vk::DescriptorPoolResetFlags());
    freePools.push_back(pool);
  }
  usedPools.clear();
  currentPool = nullptr;
}

bool DescriptorAllocator::allocate(vk::DescriptorSet *set, vk::DescriptorSetLayout layout) {

  if (!currentPool) {
    currentPool = grab_pool();
    usedPools.push_back(currentPool);
  }

  vk::DescriptorSetAllocateInfo alloc_info{currentPool, 1, &layout};

  vk::Result allocResult = logical_device.allocateDescriptorSets(&alloc_info, set);
  bool realloc_required = false;

  switch (allocResult) {
    case vk::Result::eSuccess :return true;
    case vk::Result::eErrorFragmentedPool:
    case vk::Result::eErrorOutOfPoolMemory:realloc_required = true;
      break;
    default:
      //unexpected error
      return false;
  }

  if (realloc_required) {
    currentPool = grab_pool();
    usedPools.push_back(currentPool);
    alloc_info.descriptorPool = currentPool;
    allocResult = logical_device.allocateDescriptorSets(&alloc_info, set);
  }
  return (allocResult==vk::Result::eSuccess);
}

vk::DescriptorPool createPool(vk::Device device,
                              const DescriptorAllocator::PoolSizes &pool_sizes,
                              int count,
                              vk::DescriptorPoolCreateFlags flags) {
  std::vector<vk::DescriptorPoolSize> sizes;
  sizes.reserve(pool_sizes.data.size());

  for (const auto size_entry : pool_sizes.data)
    sizes.emplace_back(size_entry.first, static_cast<uint32_t>(size_entry.second*count));

  vk::DescriptorPoolCreateInfo
      pool_info = {flags, static_cast<uint32_t>(count), static_cast<uint32_t>(sizes.size()), sizes.data()};

  return device.createDescriptorPool(pool_info);
}

vk::DescriptorPool DescriptorAllocator::grab_pool() {

  if (freePools.empty()) {
    return createPool(logical_device, descriptorSizes, 1000, vk::DescriptorPoolCreateFlags());
  }

  VkDescriptorPool pool = freePools.back();
  freePools.pop_back();
  return pool;
}

void DescriptorAllocator::init(vk::Device device) {
  logical_device = device;
}

void DescriptorAllocator::cleanup() {
  for (auto &pool : freePools) logical_device.destroy(pool);
  for (auto &pool : usedPools) logical_device.destroy(pool);
}

void DescriptorLayoutCache::init(vk::Device device) {
  logical_device = device;
}

void DescriptorLayoutCache::cleanup() {
  for (auto &cached_layout : layout_cache) logical_device.destroy(cached_layout.second);
}

vk::DescriptorSetLayout DescriptorLayoutCache::createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo *info) {

  DescriptorLayoutInfo layout_info;
  layout_info.bindings.reserve(info->bindingCount);

  bool isSorted = true;
  int lastBinding = -1;

  // copy infos into our DescriptorLayoutFormat
  for (int i = 0; i < info->bindingCount; i++) {
    layout_info.bindings.push_back(info->pBindings[i]);

    if (info->pBindings[i].binding > lastBinding) {
      lastBinding = static_cast<int>(info->pBindings[i].binding);
    } else {
      isSorted = false;
    }
  }

  // sort if not already sorted
  if (!isSorted) {
    //we use this lambda function to compare the vector elements
    std::sort(layout_info.bindings.begin(), layout_info.bindings.end(),
              [](const vk::DescriptorSetLayoutBinding &a, const vk::DescriptorSetLayoutBinding &b) {
                return a.binding < b.binding;
              });
  }

  // get descriptor layout from cache if available
  auto iter = layout_cache.find(layout_info);
  if (iter!=layout_cache.end()) {
    return iter->second;
  } else {
    vk::DescriptorSetLayout new_layout = logical_device.createDescriptorSetLayout(*info);
    layout_cache[layout_info] = new_layout;
    return new_layout;
  }

}
bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutCache::DescriptorLayoutInfo &rhs) const {
  if (rhs.bindings.size()!=bindings.size()) {
    return false;
  } else {
    //compare each of the bindings is the same. Bindings are sorted so they will match
    for (int i = 0; i < bindings.size(); i++) {
      if (!(rhs.bindings[i].binding==bindings[i].binding
          && (rhs.bindings[i].descriptorType==bindings[i].descriptorType)
          && (rhs.bindings[i].descriptorCount==bindings[i].descriptorCount)
          && (rhs.bindings[i].stageFlags==bindings[i].stageFlags))) {
        return false;
      }
    }
    return true;
  }
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
  using std::size_t, std::hash;

  size_t result = hash<size_t>()(bindings.size());
  for (const auto &vk_binding : bindings) {
    size_t binding_hash = vk_binding.binding | static_cast<VkDescriptorType>(vk_binding.descriptorType) << 8
        | vk_binding.descriptorCount << 16 | static_cast<VkShaderStageFlags>(vk_binding.stageFlags) << 24;
    result ^= hash<size_t>()(binding_hash);
  }
  return result;
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache *layout_cache, DescriptorAllocator *allocator) {
  DescriptorBuilder builder;
  builder.cache = layout_cache;
  builder.alloc = allocator;
  return builder;
}

DescriptorBuilder &DescriptorBuilder::bindBuffer(uint32_t binding,
                                                 vk::DescriptorBufferInfo *bufferInfo,
                                                 vk::DescriptorType type,
                                                 vk::ShaderStageFlags stageFlags) {

  // create binding
  vk::DescriptorSetLayoutBinding new_binding{binding, type, 1,
                                             stageFlags, nullptr};

  bindings.push_back(new_binding);

  //create descriptor write

  //.dstSet ist filled in later
  vk::WriteDescriptorSet newWrite{};
  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pBufferInfo = bufferInfo;
  newWrite.dstBinding = binding;

  writes.push_back(newWrite);
  return *this;

}

DescriptorBuilder &DescriptorBuilder::bindImage(uint32_t binding,
                                                vk::DescriptorImageInfo *imageInfo,
                                                vk::DescriptorType type,
                                                vk::ShaderStageFlags stageFlags) {
  // create binding
  vk::DescriptorSetLayoutBinding new_binding{
      binding,
      type,
      1,
      stageFlags,
      nullptr};

  bindings.push_back(new_binding);

  //create descriptor write

  //.dstSet ist filled in later
  vk::WriteDescriptorSet newWrite{};
  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pImageInfo = imageInfo;
  newWrite.dstBinding = binding;

  writes.push_back(newWrite);
  return *this;
}

bool DescriptorBuilder::build(vk::DescriptorSet &set, vk::DescriptorSetLayout &layout) {
#ifdef MODES_VERSION
  vk::DescriptorSetLayoutCreateInfo layout_create_info{vk::DescriptorSetLayoutCreateFlags(),
                                                       static_cast<uint32_t>(bindings.size()),
                                                       bindings.data()};
#else
  vk::DescriptorSetLayoutCreateInfo layout_create_info{vk::DescriptorSetLayoutCreateFlags(), bindings};
#endif

  layout = cache->createDescriptorSetLayout(&layout_create_info);
  bool success = alloc->allocate(&set, layout);
  if (!success) return false;

  for (auto &write : writes)
    write.dstSet = set;

  alloc->logical_device.updateDescriptorSets(writes, nullptr);
  return true;
}

bool DescriptorBuilder::build(vk::DescriptorSet &set) {
  vk::DescriptorSetLayout layout;
  return build(set, layout);
}
}