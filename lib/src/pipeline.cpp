//
// Created by x on 11/18/22.
//

#include "pipeline.hpp"
#include <iostream>
#include <fstream>

namespace rcc {

/*
std::vector<char> PipelineBuilder::readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + filename + "!");
  }

  uint32_t file_size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(static_cast<std::streamsize>(file_size));
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(file_size));
  file.close();

  std::cout << "file size: " << buffer.size() << "\n";
  return buffer;
}

vk::ShaderModule PipelineBuilder::createShaderModule(vk::Device &logical_device, const std::vector<char> &code) {

  VkShaderModuleCreateInfo createInfo{
      .sType = static_cast<VkStructureType>(vk::StructureType::eShaderModuleCreateInfo),
      .pNext = nullptr,
      .flags = 0,
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};

  return logical_device.createShaderModule(createInfo);
}

vk::Pipeline PipelineBuilder::buildPipeline(vk::Device &device, vk::RenderPass &pass) {
  vk::PipelineViewportStateCreateInfo viewport_state_create_info{
      vk::PipelineViewportStateCreateFlags(),
      1,
      &viewport,
      1,
      &scissor};

  vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{
      vk::PipelineColorBlendStateCreateFlags(),
      VK_FALSE,
      vk::LogicOp::eCopy,
      1,
      &colorBlendAttachment};

  vk::GraphicsPipelineCreateInfo pipeline_create_info = {
      vk::PipelineCreateFlags(),
      static_cast<uint32_t>(shaders_stage_infos.size()),
      shaders_stage_infos.data(),
      &vertex_input_info,
      &input_assembly_info,
      nullptr,
      &viewport_state_create_info,
      &rasterizer,
      &multisampling,
      &depth_stencil,
      &color_blend_state_create_info,
      nullptr,
      pipeline_layout_,
      pass,
      0,
      nullptr,
      0};

#ifdef MODES_VERSION
  vk::Pipeline new_pipeline = device.createGraphicsPipeline(nullptr, pipeline_create_info);
  return new_pipeline;
#else
  vk::ResultValue<vk::Pipeline> pipelineCreationResult = device.createGraphicsPipeline(nullptr, pipeline_create_info);
  return pipelineCreationResult.value;
#endif

}

vk::PipelineShaderStageCreateInfo PipelineBuilder::pipelineShaderStageCreateInfo(
    vk::ShaderStageFlagBits stage,
    vk::ShaderModule &module
) {

  return vk::PipelineShaderStageCreateInfo{
      vk::PipelineShaderStageCreateFlags(),
      stage,
      module,
      "main"};
}

vk::PipelineVertexInputStateCreateInfo PipelineBuilder::vertexInputStateCreateInfo() {
  return vk::PipelineVertexInputStateCreateInfo{
      vk::PipelineVertexInputStateCreateFlags(),
      0,
      nullptr,
      0,
      nullptr};
}

vk::PipelineInputAssemblyStateCreateInfo PipelineBuilder::inputAssemblyCreateInfo(vk::PrimitiveTopology topology) {
  return vk::PipelineInputAssemblyStateCreateInfo{
      vk::PipelineInputAssemblyStateCreateFlags(),
      topology,
      VK_FALSE};
}

vk::PipelineRasterizationStateCreateInfo PipelineBuilder::pipelineRasterizationStateCreateInfo(vk::PolygonMode polygon_mode) {
  return vk::PipelineRasterizationStateCreateInfo{
      vk::PipelineRasterizationStateCreateFlags(),
      VK_FALSE,
      VK_FALSE,
      polygon_mode,
      vk::CullModeFlagBits::eNone,
      vk::FrontFace::eClockwise,
      VK_FALSE,
      0.0f,
      0.0f,
      0.0f,
      1.f};
}
vk::PipelineMultisampleStateCreateInfo PipelineBuilder::pipelineMultisampleStateCreateInfo(
    vk::SampleCountFlagBits sample_count) {

  return vk::PipelineMultisampleStateCreateInfo{
      vk::PipelineMultisampleStateCreateFlags(),
      sample_count,
      VK_FALSE,
      1.f,
      nullptr,
      VK_FALSE,
      VK_FALSE};
}
vk::PipelineColorBlendAttachmentState PipelineBuilder::pipelineColorBlendAttachmentState() {
  return vk::PipelineColorBlendAttachmentState{
      VK_FALSE,
      vk::BlendFactor{},
      vk::BlendFactor{},
      vk::BlendOp{},
      vk::BlendFactor{},
      vk::BlendFactor{},
      vk::BlendOp{},
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
}
vk::PipelineLayoutCreateInfo PipelineBuilder::pipelineLayoutCreateInfo() {
  return vk::PipelineLayoutCreateInfo{
      vk::PipelineLayoutCreateFlags(),
      0,
      nullptr,
      0,
      nullptr};
}

vk::PipelineDepthStencilStateCreateInfo PipelineBuilder::depthStencilCreateInfo(bool do_depth_test,
                                                                                bool do_depth_write,
                                                                                vk::CompareOp compare_op) {
  return vk::PipelineDepthStencilStateCreateInfo{
      vk::PipelineDepthStencilStateCreateFlags(),
      static_cast<vk::Bool32>(do_depth_test ? VK_TRUE : VK_FALSE),
      static_cast<vk::Bool32>(do_depth_write ? VK_TRUE : VK_FALSE),
      do_depth_test ? compare_op : vk::CompareOp::eAlways,
      VK_FALSE,
      VK_FALSE,
      VkStencilOpState{},
      VkStencilOpState{},
      0.0f,
      1.0f};
}
*/

std::vector<char> Pipeline::readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename + "!");
    }

    uint32_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(static_cast<std::streamsize>(file_size));
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));
    file.close();

    return buffer;
}

vk::ShaderModule Pipeline::createShaderModule(vk::Device &logical_device, const std::vector<char> &code) {

    VkShaderModuleCreateInfo createInfo{
        .sType = static_cast<VkStructureType>(vk::StructureType::eShaderModuleCreateInfo),
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    return logical_device.createShaderModule(createInfo);
}

Pipeline::~Pipeline() {
    device_.destroy(pipeline_);
}

Pipeline::Pipeline(vk::Device &device,
                   const PipelineConfig &pipelineConfig,
                   const std::string &vertShaderFilepath,
                   const std::string &fragShaderFilepath,
                   vk::SpecializationInfo *const vertexSpecializationInfo /*= nullptr*/,
                   vk::SpecializationInfo *const fragmentSpecializationInfo /*= nullptr*/
) : device_(device) {

    // Create Shader Modules
    vk::ShaderModule vertexShader = createShaderModule(device_, readFile(vertShaderFilepath));
    vk::ShaderModule fragmentShader = createShaderModule(device_, readFile(fragShaderFilepath));
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStageInfos;

    shaderStageInfos.emplace_back(vk::PipelineShaderStageCreateFlags(),
                                  vk::ShaderStageFlagBits::eVertex,
                                  vertexShader,
                                  "main",
                                  vertexSpecializationInfo);
    shaderStageInfos.emplace_back(vk::PipelineShaderStageCreateFlags(),
                                  vk::ShaderStageFlagBits::eFragment,
                                  fragmentShader,
                                  "main",
                                  fragmentSpecializationInfo);

    //Create Vertex Input Infos
    auto &attributeDescriptions = pipelineConfig.attributeDescriptions;
    auto &bindingDescriptions = pipelineConfig.bindingDescriptions;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{{}, bindingDescriptions, attributeDescriptions};

    // Create Graphics Pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo{
        vk::PipelineCreateFlags(),
        shaderStageInfos,
        &vertexInputInfo,
        &pipelineConfig.inputAssemblyInfo,
        nullptr,
        &pipelineConfig.viewportInfo,
        &pipelineConfig.rasterizationInfo,
        &pipelineConfig.multisampleInfo,
        &pipelineConfig.depthStencilInfo,
        &pipelineConfig.colorBlendInfo,
        &pipelineConfig.dynamicStateInfo,
        pipelineConfig.pipelineLayout,
        pipelineConfig.renderPass,
        pipelineConfig.subpass
    };

    pipeline_ = device_.createGraphicsPipeline(nullptr, pipelineInfo).value;
    // Shader Modules won't be reused so we delete them
    device_.destroy(vertexShader);
    device_.destroy(fragmentShader);
}

void Pipeline::defaultPipelineConfigInfo(PipelineConfig &info) {
    info.inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo{
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleList,
        false
    };

    info.viewportInfo = vk::PipelineViewportStateCreateInfo{
        vk::PipelineViewportStateCreateFlags(),
        1, nullptr, 1, nullptr, // 1 viewport and 1 scissor placeholder
    };

    info.rasterizationInfo = vk::PipelineRasterizationStateCreateInfo{
        vk::PipelineRasterizationStateCreateFlags(),
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone,
        vk::FrontFace::eClockwise,
        false,
        0, 0, 0,
        1.f
    };

    info.multisampleInfo = vk::PipelineMultisampleStateCreateInfo{
        vk::PipelineMultisampleStateCreateFlags(),
        vk::SampleCountFlagBits::e1,
        false,
        1.f,
        nullptr,
        false,
        false
    };

    {
        using bf = vk::BlendFactor;
        using bo = vk::BlendOp;
        using cb = vk::ColorComponentFlagBits;
        //vk::ColorComponentFlagBits;
        info.colorBlendAttachment = std::vector<vk::PipelineColorBlendAttachmentState>(1,{
            false, bf::eZero, bf::eZero, bo::eAdd, bf::eZero, bf::eZero, bo::eAdd,
            cb::eR | cb::eG | cb::eB | cb::eA});
        info.colorBlendInfo = vk::PipelineColorBlendStateCreateInfo{
            vk::PipelineColorBlendStateCreateFlags(),
            false,
            vk::LogicOp::eCopy,
            info.colorBlendAttachment
        };
    }

    info.depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo{
        vk::PipelineDepthStencilStateCreateFlags(),
        true,
        true,
        vk::CompareOp::eLess,
        false,
        false
    };

    info.dynamicStateEnables = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    info.dynamicStateInfo = vk::PipelineDynamicStateCreateInfo{
        vk::PipelineDynamicStateCreateFlags(),
        info.dynamicStateEnables
    };

}

}