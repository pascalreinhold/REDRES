//
// Created by x on 11/18/22.
//

#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string>

namespace rcc {

struct PipelineConfig {
  PipelineConfig() = default;
  PipelineConfig(const PipelineConfig &) = delete;
  PipelineConfig &operator=(const PipelineConfig &) = delete;

  std::vector<vk::VertexInputBindingDescription> bindingDescriptions{};
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
  vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
  vk::PipelineMultisampleStateCreateInfo multisampleInfo;
  vk::PipelineColorBlendAttachmentState colorBlendAttachment; // needed for blend info
  vk::PipelineColorBlendStateCreateInfo colorBlendInfo;
  vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;

  // Those are (partially) null initialized be default config
  vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
  std::vector<vk::DynamicState> dynamicStateEnables{};
  vk::PipelineViewportStateCreateInfo viewportInfo; // viewPort and Scissor have to be set
  vk::PipelineLayout pipelineLayout = nullptr;
  vk::RenderPass renderPass = nullptr;
  uint32_t subpass = 0;
};

class Pipeline {
 public:

  Pipeline(
      vk::Device &device,
      const PipelineConfig &pipelineConfig,
      const std::string &vertShaderFilepath,
      const std::string &fragShaderFilepath,
      vk::SpecializationInfo *vertexSpecializationInfo = nullptr,
      vk::SpecializationInfo *fragmentSpecializationInfo = nullptr);

  ~Pipeline();

  vk::Pipeline &pipeline() { return pipeline_; }

  //not copyable or assignable
  Pipeline(const Pipeline &) = delete;
  Pipeline &operator=(const Pipeline &) = delete;

  static void defaultPipelineConfigInfo(PipelineConfig &info);
  static std::vector<char> readFile(const std::string &filename);
  static vk::ShaderModule createShaderModule(vk::Device &logical_device, const std::vector<char> &code);

 private:

  vk::Device &device_;
  vk::Pipeline pipeline_;
};
}