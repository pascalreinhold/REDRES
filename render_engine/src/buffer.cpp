#include "buffer.hpp"
#include "mesh.hpp"
#include <iostream>

namespace rcc {

uint32_t ResourceManager::next_handle_ = 0;

ResourceManager::ResourceManager(vk::Device &device, VmaAllocator &allocator):
    device_{device}, allocator_{allocator} {
}

ResourceManager::~ResourceManager() {
    for (auto& [handle, buffer] : buffers_) {
        destroyBuffer(buffer);
    }
}

uint32_t ResourceManager::createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) {
    return createBuffer(size, buffer_usage, memory_usage, vk::SharingMode::eExclusive, {});
}

uint32_t ResourceManager::createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage,
                                       vk::SharingMode sharing_mode, const std::vector<uint32_t>& queue_family_indices){

    auto insert_result = buffers_.emplace(next_handle_, Buffer{});
    assert(insert_result.second); //check if the handle is already in use
    auto& buffer = insert_result.first->second;

    buffer.size_ = size;
    buffer.buffer_usage_ = buffer_usage;
    buffer.memory_usage_ = memory_usage;

    vk::BufferCreateInfo buffer_create_info{vk::BufferCreateFlags(), size, buffer_usage, sharing_mode, queue_family_indices};

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = memory_usage;
    vmaCreateBuffer(allocator_,
                    &static_cast<const VkBufferCreateInfo&>(buffer_create_info),
                    &static_cast<const VmaAllocationCreateInfo&>(allocation_create_info),
                    reinterpret_cast<VkBuffer*>(&buffer.buffer_), &buffer.allocation_, nullptr);

    return next_handle_++;
}




BufferResource ResourceManager::createBufferResource(uint32_t buffer_handle,
                                                     vk::DeviceSize offset,
                                                     vk::DeviceSize range,
                                                     vk::DescriptorType descriptor_type) {

    range = (range == VK_WHOLE_SIZE) ? buffers_.at(buffer_handle).size_ : range;
    auto& buffer = buffers_.at(buffer_handle);
    assert(buffer.size_ >= offset + range);

    BufferResource buffer_resource{};
    buffer_resource.handle_ = buffer_handle;
    buffer_resource.descriptor_type_ = descriptor_type;
    buffer_resource.descriptor_buffer_info_ = vk::DescriptorBufferInfo{buffer.buffer_, offset, range};
    return buffer_resource;
}


void *ResourceManager::getMappedData(Buffer &buffer) {
    return buffer.mapped_data_;
}

void *ResourceManager::getMappedData(uint32_t buffer_handle) {
    return getMappedData(buffers_.at(buffer_handle));
}

void ResourceManager::mapBuffer(Buffer &buffer) {
    vmaMapMemory(allocator_, buffer.allocation_, &buffer.mapped_data_);
}

void ResourceManager::mapBuffer(uint32_t buffer_handle) {
    mapBuffer(buffers_.at(buffer_handle));
}

void ResourceManager::unmapBuffer(Buffer &buffer) {
    vmaUnmapMemory(allocator_, buffer.allocation_);
    buffer.mapped_data_ = nullptr;
}
void ResourceManager::unmapBuffer(uint32_t buffer_handle) {
    unmapBuffer(buffers_.at(buffer_handle));
}
void ResourceManager::destroyBuffer(uint32_t buffer_handle) {
    destroyBuffer(buffers_.at(buffer_handle));
}

void ResourceManager::destroyBuffer(Buffer &buffer) {
    if(buffer.mapped_data_) {
        vmaUnmapMemory(allocator_, buffer.allocation_);
    }
    vmaDestroyBuffer(allocator_, buffer.buffer_, buffer.allocation_);
}

void ResourceManager::writeToBuffer(BufferResource buffer_resource, const void *data, vk::DeviceSize size) {
    Buffer& buffer = buffers_.at(buffer_resource.handle_);
    assert(buffer.mapped_data_ != nullptr);

    void* final_location = reinterpret_cast<char*>(buffer.mapped_data_) + buffer_resource.descriptor_buffer_info_.offset;
    memcpy(final_location, data, size);
}

void ResourceManager::immediateSubmit(UploadContext& upload_context, vk::Queue& upload_queue, std::function<void(vk::CommandBuffer cmd)> &&function) {
    vk::CommandBuffer cmd = upload_context.command_buffer;
    vk::CommandBufferBeginInfo cmd_begin_info{vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr};

    cmd.begin(cmd_begin_info);
    function(cmd);
    cmd.end();
    vk::SubmitInfo submit_info{0, nullptr, nullptr, 1, &cmd, 0, nullptr};

    upload_queue.submit(submit_info, upload_context.upload_fence);
    const auto fence_wait_result = device_.waitForFences(upload_context.upload_fence, true, 10'000'000'000);
    if (fence_wait_result!=vk::Result::eSuccess) abort();
    device_.resetFences(upload_context.upload_fence);
    device_.resetCommandPool(upload_context.command_pool);
}


void ResourceManager::stageBuffer(void *src, vk::DeviceSize size, BufferResource dest_resource, UploadContext& upload_context, vk::Queue& upload_queue) {

    auto staging_buffer_handle_ = createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);
    BufferResource staging_buffer_resource = createBufferResource(staging_buffer_handle_, 0, size, vk::DescriptorType::eStorageBuffer);
    auto& staging_buffer = buffers_.at(staging_buffer_resource.handle_);

    mapBuffer(staging_buffer_resource.handle_);
    writeToBuffer(staging_buffer_resource, src, size);
    auto& dest_buffer = buffers_.at(dest_resource.handle_);

    assert(dest_buffer.size_ >= dest_resource.descriptor_buffer_info_.offset + size);
    immediateSubmit(
        upload_context,
        upload_queue,
        [=](vk::CommandBuffer cmd) {
        vk::BufferCopy copy{0, dest_resource.descriptor_buffer_info_.offset, size};
        cmd.copyBuffer(staging_buffer.buffer_, dest_buffer.buffer_, 1, &copy);
    });

    destroyBuffer(staging_buffer_resource.handle_);
    buffers_.erase(staging_buffer_resource.handle_);
}

void ResourceManager::readFromBuffer(BufferResource &buffer, uint32_t range, void *data) {
    char *mapped = (char*) getMappedData(buffer.handle_);
    mapped += buffer.descriptor_buffer_info_.offset;
    memcpy(data, mapped, range);
}

void ResourceManager::readFromBufferAndClearIt(BufferResource &buffer, uint32_t range, void *data) {
    char *mapped = (char*) getMappedData(buffer.handle_);
    mapped += buffer.descriptor_buffer_info_.offset;
    memcpy(data, mapped, range);
    memset(mapped, 0, range);
}
Buffer &ResourceManager::getBuffer(uint32_t buffer_handle) {
    return buffers_.at(buffer_handle);
}

Buffer &ResourceManager::getBuffer(BufferResource buffer_resource) {
    return getBuffer(buffer_resource.handle_);
}

void ResourceManager::clearBuffer(BufferResource buffer_resource) {
    void* mapped = getMappedData(buffer_resource.handle_);
    assert(mapped);
    assert(buffer_resource.descriptor_buffer_info_.range + buffer_resource.descriptor_buffer_info_.offset
    <= buffers_.at(buffer_resource.handle_).size_);

    memset(mapped, static_cast<int>(buffer_resource.descriptor_buffer_info_.offset), buffer_resource.descriptor_buffer_info_.range);
}


std::pair<BufferResource, BufferResource> ResourceManager::uploadMesh(Mesh &mesh, UploadContext upload_context_, vk::Queue upload_queue_) {

    size_t vertex_buffer_size = mesh.vertices_.size()*sizeof(BasicVertex);
    //print the value type of the vertices vector


    auto vertex_buffer_handle_ = createBuffer(
        vertex_buffer_size,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        VMA_MEMORY_USAGE_GPU_ONLY);
    auto vertex_buffer_resource = createBufferResource(vertex_buffer_handle_, 0, vertex_buffer_size, {});
    stageBuffer(mesh.vertices_.data(), vertex_buffer_size, vertex_buffer_resource, upload_context_, upload_queue_);

    size_t index_buffer_size = mesh.indices_.size()*sizeof(uint32_t);
    auto index_buffer_handle_ = createBuffer(
        index_buffer_size,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        VMA_MEMORY_USAGE_GPU_ONLY);
    auto index_buffer_resource = createBufferResource(index_buffer_handle_, 0, index_buffer_size, {});
    stageBuffer(mesh.indices_.data(), index_buffer_size, index_buffer_resource, upload_context_, upload_queue_);

    return {vertex_buffer_resource, index_buffer_resource};
}


} // namespace rcc