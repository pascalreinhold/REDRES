#include "engine.hpp"
#include "gui.hpp"
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "VkBootstrap.h"

#include <array>
#include <cmath>
#include <thread>
#include <iostream>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>

namespace {
glm::vec4 normalizePlane(const glm::vec4 &plane) {
  return plane/glm::length(glm::vec3(plane));
}
}

namespace rcc {

static constexpr uint32_t GetVulkanApiVersion() {
#if VMA_VULKAN_VERSION==1003000
  return VK_API_VERSION_1_3;
#elif VMA_VULKAN_VERSION==1002000
  return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION==1001000
  return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
    return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
    return UINT32_MAX;
#endif
}

Engine *Engine::keyboardBackedEngine{nullptr};

Engine::Engine(const char *db_filepath, const char *asset_dir_path) : db_filepath_(db_filepath) {
  //needed to get feedback from inside glfw callback functions
  Engine::keyboardBackedEngine = this;

  // if a directory path was given overwrite the default one
  if (asset_dir_path) asset_dir_filepath_ = std::string(asset_dir_path);

  try {
    // try to read non-default settings json (does not exist on the first app start or might be corrupted on a crash)
    std::ifstream json_stream(asset_dir_filepath_ + settings_filepath_);
    getConfig() = nlohmann::json::parse(json_stream);
    json_stream.close();
  } catch (std::exception &e) {
    //read the default settings
    std::ifstream json_stream(asset_dir_filepath_ + default_settings_filepath_);
    getConfig() = nlohmann::json::parse(json_stream);
    json_stream.close();
  }

  getConfig()["AssetDirectoryFilepath"] = asset_dir_filepath_;

  clearColor = getConfig()["ClearColor"].get<std::array<float, 4>>();
  max_cell_count_ = getConfig()["MaxCellCount"].get<int>();
  framerate_control_.movie_framerate_ = Engine::getConfig()["MovieFrameRate"].get<int>();

  //window creation
  std::string windowName = getConfig()["WindowName"].get<std::string>();
  int width = getConfig()["WindowWidth"].get<int>();
  int height = getConfig()["WindowHeight"].get<int>();
  window_ = std::make_unique<Window>(width, height, windowName);
  initGlfwCallbacks();
}

void Engine::init() {
  initVulkan();
  initCamera();
  recreateSwapchain();
  initCommands();
  initSyncStructures();
  initDescriptors();
  initPipelines();
  initVisData();
  initScene();
  initComputePipelines();
  ui = std::make_unique<UserInterface>(this);
}

void Engine::initVulkan() {
  vkb::InstanceBuilder instanceBuilder;

#ifdef NDEBUG
  const bool enableValidationLayers = true;
#else
  const bool enableValidationLayers = true;
#endif

  vkb::Instance vkbInstance = instanceBuilder
      .set_app_name("TOFHED")
      .set_engine_name("Renderer For Computational Chemistry - RCC")
      .require_api_version(1, 1, 0)
      .request_validation_layers(enableValidationLayers)
      .use_default_debug_messenger()
      .build()
      .value();

  instance_ = vkbInstance.instance;
  debug_messenger_ = vkbInstance.debug_messenger;

  window_->createSurface(instance_, &surface_);

  vkb::PhysicalDeviceSelector selector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice = selector
      .set_minimum_version(1, 1)
      .set_surface(surface_)
      .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
      .select()
      .value();

  vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};

  VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features =
      static_cast<VkPhysicalDeviceShaderDrawParametersFeatures>(vk::PhysicalDeviceShaderDrawParametersFeatures(VK_TRUE));
  vkb::Device vkbDevice = deviceBuilder.add_pNext(&shader_draw_parameters_features).build().value();

  logical_device_ = vkbDevice.device;
  physical_device_ = vkbPhysicalDevice.physical_device;

  gpu_properties_ = vkbDevice.physical_device.properties;

  graphics_queue_ = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_ = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  static VmaVulkanFunctions vulkanFunctions = {};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.physicalDevice = physical_device_;
  allocator_create_info.device = logical_device_;
  allocator_create_info.instance = instance_;
  allocator_create_info.vulkanApiVersion = GetVulkanApiVersion();
  allocator_create_info.pVulkanFunctions = &vulkanFunctions;
  vmaCreateAllocator(&allocator_create_info, &allocator_);

  main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Allocator\n";
#endif
    vmaDestroyAllocator(allocator_);
  });
}

void Engine::initCamera() {
  //load camera-specific settings
  Camera::PerspectiveViewSettings perspective_settings{
      Engine::getConfig()["NearPlane"].get<float>(),
      Engine::getConfig()["FarPlane"].get<float>(),
      Engine::getConfig()["FOVY"].get<float>(),
      Engine::getConfig()["MovementSpeed"].get<float>(),
      Engine::getConfig()["TurnSpeed"].get<float>(),
  };
  Camera::IsometricViewSettings isometric_settings{
      Engine::getConfig()["IsometricHeight"].get<float>(),
      Engine::getConfig()["IsometricDepth"].get<float>(),
      Engine::getConfig()["ZoomSpeed"].get<float>()
  };

  camera_ = std::make_unique<Camera>(perspective_settings, isometric_settings);
  camera_->is_isometric = Engine::getConfig()["UseIsometric"];
  camera_->drag_speed_ = Engine::getConfig()["DragSpeed"];

}

void Engine::processMousePickingBuffer() {
  if (!bReadMousePickingBuffer_ || !scene_->visManager) return;
  bReadMousePickingBuffer_ = false;

  selected_object_index_ = -1;
  for (unsigned int mouse_bucket : mouse_buckets) {
    if (mouse_bucket!=0) {
      selected_object_index_ = static_cast<int>(mouse_bucket);
      break;
    }
  }

  // make sure an atom is selected
  if ((selected_object_index_!=-1) && (selected_object_index_ < (*scene_)["Atom"].MaxCount())) {

    // logic for selecting atoms for tagging mode
    if (ui_mode_==uiMode::eSelectAndTag) {
      scene_->visManager->getTagsRef()(selected_object_index_) ^= Tags::eSelectedForTagging;
    }

    // logic for selecting atoms for measurement mode
    if (ui_mode_==uiMode::eMeasure) {
      bool wasSelectedBefore = scene_->visManager->getTagsRef()(selected_object_index_) & Tags::eSelectedForMeasurement;
      scene_->visManager->getTagsRef()(selected_object_index_) |= Tags::eSelectedForMeasurement;

      if(!wasSelectedBefore) selected_atom_numbers_.push_back(selected_object_index_);

      if(selected_atom_numbers_.size() > 3) {
        scene_->visManager->getTagsRef()(selected_atom_numbers_.front()) ^= Tags::eSelectedForMeasurement;
        selected_atom_numbers_.pop_front();
      }
    }
  } else {
    cleanupMeasurementMode();
  }
}

void Engine::processMouseDrag() {
  static std::array<double, 2> last_pos{0.f, 0.f};
  std::array<double, 2> current_pos{0.f, 0.f};
  std::array<double, 2> offset{};
  glfwGetCursorPos(window_->glfwWindow_, &current_pos[0], &current_pos[1]);
  offset[0] = (current_pos[0] - last_pos[0])/window_->width()*2.*M_PI;
  offset[1] = (current_pos[1] - last_pos[1])/window_->height()*2.*M_PI;
  last_pos = current_pos;

  if (bDragRotateCam_) {
    //if we don't use the isometric camera we want to inverse the rotation on the viewing direction
    if (!camera_->is_isometric) {
      offset[0] = -offset[0];
      offset[1] = -offset[1];
    }

    float dx = camera_->drag_speed_*static_cast<float>(framerate_control_.frame_time_);

//rotate the camera
    const glm::vec3 right_direction{glm::normalize(glm::cross(camera_->view_direction_, camera_->up_direction_))};
    glm::mat3 rightDirectionRotationMat = glm::mat3(glm::rotate(-static_cast<float>(offset[1]*dx), right_direction));
    camera_->up_direction_ = glm::normalize(rightDirectionRotationMat*camera_->up_direction_);
    glm::mat3
        upDirectionRotationMat = glm::mat3(glm::rotate(-static_cast<float>(offset[0]*dx), camera_->up_direction_));
    camera_->view_direction_ =
        glm::normalize(rightDirectionRotationMat*upDirectionRotationMat*camera_->view_direction_);
  }

}

void Engine::run() {
  while (!window_->shouldClose()) {
    glfwPollEvents();

    //read buffers from last frame
    long int frame_index = ((framerate_control_.frame_number_ - 1)%FRAMES_IN_FLIGHT);
    if (frame_index==-1) frame_index = FRAMES_IN_FLIGHT - 1;

    readFromBufferAndClearIt(frame_data_[frame_index].mouseBucketBuffer,
                             sizeof(uint32_t)*RCC_MOUSE_BUCKET_COUNT,
                             mouse_buckets);

    if (scene_->visManager) {
      // update selected index variable
      processMousePickingBuffer();
      processMouseDrag();
      if (!framerate_control_.manualFrameControl) {
        float ms_per_frame = 1000.f/static_cast<float>(framerate_control_.movie_framerate_);
        float step = framerate_control_.frame_time_/ms_per_frame;
        framerate_control_.movie_frame_index_ += step;

        if (framerate_control_.movie_frame_index_ >= static_cast<float>(scene_->MovieFrameCount())) {
          framerate_control_.movie_frame_index_ =
              (framerate_control_.isSimulationLooped) ? 0 : static_cast<float>(scene_->MovieFrameCount() - 1);
        }
      }
    }

    ui->show();
    if(scene_->visManager) {
      if(window_->windowName() != (getConfig()["WindowName"].get<std::string>()) + " - " + scene_->visManager->getDBFilepath()) {
        window_->setWindowName((getConfig()["WindowName"].get<std::string>()) + " - " + scene_->visManager->getDBFilepath());
        //std::cout << window_->windowName() << std::endl;
      }
    } else {
      if(window_->windowName() != (getConfig()["WindowName"].get<std::string>())) {
        window_->setWindowName((getConfig()["WindowName"].get<std::string>()));
        //std::cout << window_->windowName() << std::endl;
      }
    }
    render();
    framerate_control_.frame_number_++;
  }
}

void Engine::recreateSwapchain() {
  vk::Extent2D window_extent{static_cast<uint32_t>(window_->width()), static_cast<uint32_t>(window_->height())};

  while (window_extent.width==0 || window_extent.height==0) {
    window_extent = vk::Extent2D{static_cast<uint32_t>(window_->width()), static_cast<uint32_t>(window_->height())};
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(logical_device_);

  if (swapchain_==nullptr) {
    swapchain_ = std::make_unique<Swapchain>(logical_device_,
                                             physical_device_,
                                             surface_,
                                             allocator_,
                                             window_extent);
  } else {
    std::shared_ptr<Swapchain> previousSwapchain = std::move(swapchain_);
    swapchain_ = std::make_unique<Swapchain>(logical_device_,
                                             physical_device_,
                                             surface_,
                                             allocator_,
                                             window_extent,
                                             previousSwapchain);
  }

}

void Engine::initCommands() {
  auto command_pool_create_info =
      vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphics_queue_family_);

  for (auto &frame : frame_data_) {

    frame.command_pool = logical_device_.createCommandPool(command_pool_create_info);

    const auto
        cmdBufferAllocateInfo = vk::CommandBufferAllocateInfo(frame.command_pool, vk::CommandBufferLevel::ePrimary, 1);

    frame.main_command_buffer = logical_device_.allocateCommandBuffers(cmdBufferAllocateInfo)[0];

    main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
      std::cout << "Destroying Command Pool\n";
#endif
      logical_device_.destroy(frame.command_pool);
    });
  }

  vk::CommandPoolCreateInfo upload_command_pool_create_info{vk::CommandPoolCreateFlags(), graphics_queue_family_};
  upload_context_.command_pool = logical_device_.createCommandPool(upload_command_pool_create_info);
  vk::CommandBufferAllocateInfo
      cmdBufferAllocateInfo{upload_context_.command_pool, vk::CommandBufferLevel::ePrimary, 1};
  upload_context_.command_buffer = logical_device_.allocateCommandBuffers(cmdBufferAllocateInfo)[0];

  main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Upload Context Command Pool\n";
#endif
    logical_device_.destroy(upload_context_.command_pool);
  });

}

Engine::~Engine() {
  DumpSettingsJson(asset_dir_filepath_ + settings_filepath_);
  cleanup();
}

void Engine::initSyncStructures() {

  vk::FenceCreateInfo render_fence_create_info{vk::FenceCreateFlagBits::eSignaled};
  vk::FenceCreateInfo upload_fence_create_info{vk::FenceCreateFlags()};

  vk::SemaphoreCreateInfo semaphore_create_info{vk::SemaphoreCreateFlags()};

  upload_context_.upload_fence = logical_device_.createFence(upload_fence_create_info);
  logical_device_.resetFences(upload_context_.upload_fence);

  main_destruction_stack_.push([=]() {

#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Upload Fence" << "\n";
#endif
    logical_device_.destroy(upload_context_.upload_fence);
  });

  for (auto &frame : frame_data_) {
    frame.render_fence = logical_device_.createFence(render_fence_create_info);
    frame.render_semaphore = logical_device_.createSemaphore(semaphore_create_info);
    frame.present_semaphore = logical_device_.createSemaphore(semaphore_create_info);
    main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
      std::cout << "Destroying Present and Render Semaphores and Render Fence" << "\n";
#endif
      logical_device_.destroy(frame.present_semaphore);
      logical_device_.destroy(frame.render_semaphore);
      logical_device_.destroy(frame.render_fence);
    });
  }
}

void Engine::render() {
  ui->render();

  auto new_time = std::chrono::high_resolution_clock::now();
  framerate_control_.frame_time_ = static_cast<float>(std::chrono::duration<double, std::chrono::milliseconds::period>(
      new_time - framerate_control_.currentTime).count());

  double minMs = (1000./framerate_control_.max_framerate_);
  if (framerate_control_.frame_time_ < minMs) {
    std::chrono::duration<double, std::milli> waiting_time(minMs - framerate_control_.frame_time_);
    auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(waiting_time).count();
    std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration));
  }

  framerate_control_.avgFrameTime.feed(framerate_control_.frame_time_);
  framerate_control_.currentTime = new_time;



  //acquireNextImage gives us the index of an available swapchain image we can render into
  //the semaphore given is signaled if the image was presented
  //signals a semaphore when the
  auto [result, swapchainIndex] = swapchain_->acquireNextImage(getCurrentFrame().present_semaphore);
  if (result==vk::Result::eErrorOutOfDateKHR) {
    recreateSwapchain();
    return;
  }

  // we wait on the fence before writing our buffers
  const auto fence_wait_result =
      logical_device_.waitForFences(getCurrentFrame().render_fence, true, 1'000'000'000); //timeout in nanoseconds
  if (fence_wait_result!=vk::Result::eSuccess) abort();
  logical_device_.resetFences(getCurrentFrame().render_fence);

  if (scene_->visManager) {
    //reset IndirectDrawClearBuffer if we have a new Experiment
    static int currExperimentID = -1;
    if (currExperimentID!=scene_->visManager->getActiveExperiment()) {
      currExperimentID = scene_->visManager->getActiveExperiment();
      loadMeshes();
      writeClearDrawCallBuffer();
    }
    writeIndirectDispatchBuffer();
    writeObjectAndInstanceBuffer();
    writeOffsetBuffer();
    writeCameraBuffer();
    writeSceneBuffer();
    writeCullBuffer();
  }

  // begin and record cmd buffer
  auto &cmd = getCurrentFrame().main_command_buffer;
  vk::CommandBufferBeginInfo cmd_begin_info{};
  cmd.begin(cmd_begin_info);

  if (scene_->visManager) {
    resetDrawData(cmd, clear_draw_call_buffer_, getCurrentFrame().draw_call_buffer, sizeof(GPUDrawCalls));
    runCullComputeShader(cmd);
  }

  beginRenderPass(cmd, swapchainIndex);
  if (scene_->visManager) draw(cmd);
  ui->writeDrawDataToCmdBuffer(cmd);
  cmd.endRenderPass();
  cmd.end();

  {
    // Wait in the command buffer at the pipeline stage eColorAttachmentOutput until the image was acquired
    // signal render fence and render_semaphore when the submitted command buffer is done
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info
        {1, &getCurrentFrame().present_semaphore, &waitStage, 1, &cmd, 1, &getCurrentFrame().render_semaphore};
    graphics_queue_.submit(1, &submit_info, getCurrentFrame().render_fence);
  }

  //queue the rendered image for presentation
  //waits on the render semaphore, which is signaled on cmd buffer has been worked through
  result = swapchain_->present(swapchainIndex, getCurrentFrame().render_semaphore, graphics_queue_);

  //recreate swapchain if necessary
  if (result==vk::Result::eErrorOutOfDateKHR || result==vk::Result::eSuboptimalKHR || window_->wasResized()) {
    window_->resetWasResizedFlag();
    recreateSwapchain();
  } else if (result!=vk::Result::eSuccess) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void Engine::draw(vk::CommandBuffer &cmd) const {

  //bind Global Descriptor Set and Vertex-/Index-Buffers
  uint32_t gpu_ubo_offset = paddedUniformBufferSize(sizeof(GPUSceneData))*getCurrentFrameIndex();

  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_layout_,
                         0, 1, &getCurrentFrame().globalDescriptorSet,
                         1, &gpu_ubo_offset);

  cmd.bindVertexBuffers(0, static_cast<vk::Buffer>(meshes.vertexBuffer.buffer), static_cast<vk::DeviceSize>(0));
  cmd.bindIndexBuffer(meshes.indexBuffer.buffer, 0, vk::IndexType::eUint32);

  if (camera_->is_isometric) {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, atom_pipeline_iso_->pipeline());
  } else {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, atom_pipeline_->pipeline());
  }

  if ((*scene_)["Atom"].isLoaded() && (*scene_)["Atom"].shown) {
    cmd.drawIndexedIndirect(getCurrentFrame().draw_call_buffer.buffer,
                            sizeof(vk::DrawIndexedIndirectCommand)*meshID::eAtom,
                            1,
                            sizeof(vk::DrawIndexedIndirectCommand));
  }

  if ((*scene_)["UnitCell"].isLoaded() && (*scene_)["UnitCell"].shown) {
    cmd.drawIndexedIndirect(getCurrentFrame().draw_call_buffer.buffer,
                            sizeof(vk::DrawIndexedIndirectCommand)*meshID::eUnitCell,
                            1,
                            sizeof(vk::DrawIndexedIndirectCommand));
  }

  if ((*scene_)["Vector"].isLoaded() && (*scene_)["Vector"].shown) {
    cmd.drawIndexedIndirect(getCurrentFrame().draw_call_buffer.buffer,
                            sizeof(vk::DrawIndexedIndirectCommand)*meshID::eVector,
                            1,
                            sizeof(vk::DrawIndexedIndirectCommand));
  }

  if ((*scene_)["Cylinder"].isLoaded() && (*scene_)["Cylinder"].shown) {
    cmd.drawIndexedIndirect(getCurrentFrame().draw_call_buffer.buffer,
                            sizeof(vk::DrawIndexedIndirectCommand)*meshID::eCylinder,
                            1,
                            sizeof(vk::DrawIndexedIndirectCommand));
  }

  if (camera_->is_isometric) {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, bond_pipeline_iso_->pipeline());
  } else {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, bond_pipeline_->pipeline());
  }

  if ((*scene_)["Bond"].isLoaded() && (*scene_)["Bond"].shown) {
    cmd.drawIndexedIndirect(getCurrentFrame().draw_call_buffer.buffer,
                            sizeof(vk::DrawIndexedIndirectCommand)*meshID::eBond,
                            1,
                            sizeof(vk::DrawIndexedIndirectCommand));
  }
}

void Engine::runCullComputeShader(vk::CommandBuffer cmd) {
  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, culling_compute_pipeline_);
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                         culling_compute_pipeline_layout_,
                         0,
                         getCurrentFrame().test_compute_shader_set,
                         {});

  cmd.dispatchIndirect(indirect_dispatch_buffer_.buffer, 0);

  std::vector<vk::BufferMemoryBarrier> barriers = {
      vk::BufferMemoryBarrier{vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead,
                              graphics_queue_family_, graphics_queue_family_,
                              getCurrentFrame().final_instance_buffer.buffer, 0,
                              MAX_UNIQUE_OBJECTS*sizeof(GPUFinalInstance)*27},
      vk::BufferMemoryBarrier{vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
                              vk::AccessFlagBits::eMemoryRead,
                              graphics_queue_family_, graphics_queue_family_,
                              getCurrentFrame().draw_call_buffer.buffer, 0,
                              sizeof(GPUDrawCalls)}
  };

  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader,
                      vk::DependencyFlags(), nullptr, barriers, nullptr);
}

void Engine::cleanup() {
  logical_device_.waitIdle();
  swapchain_.reset();
  main_destruction_stack_.flush();
  descriptor_allocator_.cleanup();
  layout_cache_.cleanup();

  atom_pipeline_.reset(nullptr);
  bond_pipeline_.reset(nullptr);
  atom_pipeline_iso_.reset(nullptr);
  bond_pipeline_iso_.reset(nullptr);

  instance_.destroy(surface_);
  logical_device_.destroy();
  vkb::destroy_debug_utils_messenger(instance_, debug_messenger_);
  instance_.destroy();
}

void Engine::initPipelines() {
  // Create Pipeline Layouts
  vk::PipelineLayoutCreateInfo atom_bond_pipeline_layout_info{
      vk::PipelineLayoutCreateFlags(), graphics_descriptor_set_layout
  };
  graphics_pipeline_layout_ = logical_device_.createPipelineLayout(atom_bond_pipeline_layout_info);

  main_destruction_stack_.push([=]() {

#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Graphics Pipeline Layouts" << "\n";
#endif
    logical_device_.destroy(graphics_pipeline_layout_);
  });

  //create default shaders;
  const std::string atom_vs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["AtomVertexShaderFilepath"].get<std::string>();
  const std::string atom_fs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["AtomFragmentShaderFilepath"].get<std::string>();
  const std::string bond_vs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["BondVertexShaderFilepath"].get<std::string>();
  const std::string bond_fs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["BondFragmentShaderFilepath"].get<std::string>();
  const std::string atom_fs_iso_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["AtomFragmentShaderIsoFilepath"].get<std::string>();
  const std::string bond_fs_iso_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + getConfig()["BondFragmentShaderIsoFilepath"].get<std::string>();

  PipelineConfig config{};
  Pipeline::defaultPipelineConfigInfo(config);
  config.renderPass = swapchain_->renderPass();
  config.pipelineLayout = graphics_pipeline_layout_;

  VertexDescription vertexDescription = BasicVertex::getDescription();
  config.bindingDescriptions = vertexDescription.bindings_;
  config.attributeDescriptions = vertexDescription.attributes_;

  // Atom Pipeline Creation
  std::vector<vk::SpecializationMapEntry> atom_fs_specializations;
  atom_fs_specializations.emplace_back(0, offsetof(SpecializationConstants, point_light_count), sizeof(uint32_t));
  atom_fs_specializations.emplace_back(1, offsetof(SpecializationConstants, mouse_bucket_count), sizeof(uint32_t));
  vk::SpecializationInfo
      atomFsSpecializationInfo{static_cast<uint32_t>(atom_fs_specializations.size()), atom_fs_specializations.data(),
                               sizeof(specialization_constants_), &specialization_constants_};

  atom_pipeline_ = std::make_unique<Pipeline>(logical_device_,
                                              config,
                                              atom_vs_path,
                                              atom_fs_path,
                                              nullptr,
                                              &atomFsSpecializationInfo);
  atom_pipeline_iso_ = std::make_unique<Pipeline>(logical_device_,
                                                  config,
                                                  atom_vs_path,
                                                  atom_fs_iso_path,
                                                  nullptr,
                                                  &atomFsSpecializationInfo);


  // Bond Pipeline Creation
  std::vector<vk::SpecializationMapEntry> bond_fs_specializations;
  bond_fs_specializations.emplace_back(0, offsetof(SpecializationConstants, point_light_count), sizeof(uint32_t));
  vk::SpecializationInfo
      bondFsSpecializationInfo{static_cast<uint32_t>(bond_fs_specializations.size()), bond_fs_specializations.data(),
                               sizeof(specialization_constants_), &specialization_constants_};
  bond_pipeline_ = std::make_unique<Pipeline>(logical_device_,
                                              config,
                                              bond_vs_path,
                                              bond_fs_path,
                                              nullptr,
                                              &bondFsSpecializationInfo);
  bond_pipeline_iso_ = std::make_unique<Pipeline>(logical_device_,
                                                  config,
                                                  bond_vs_path,
                                                  bond_fs_iso_path,
                                                  nullptr,
                                                  &bondFsSpecializationInfo);
}

void Engine::initScene() {

  auto &reciprocal_gamma = scene_data_.params[0][0];
  auto &shininess = scene_data_.params[0][1];
  auto &diffuse_coeff = scene_data_.params[0][2];
  auto &specular_coeff = scene_data_.params[0][3];

  reciprocal_gamma = Engine::getConfig()["Reciprocal Gamma"].get<float>();
  shininess = Engine::getConfig()["Shininess"].get<float>();
  diffuse_coeff = Engine::getConfig()["Diffuse Coeff"].get<float>();
  specular_coeff = Engine::getConfig()["Specular Coeff"].get<float>();

  for (auto &params : scene_data_.params) {
    params = scene_data_.params[0]; // this makes one self assignment but I digress
  }

}

void Engine::loadMeshes() {
  Mesh atom_mesh, unit_cell_mesh, vector_mesh, cylinder_mesh, bond_mesh;

  unit_cell_mesh.createUnitCellMesh(scene_->cellGLM());
  unit_cell_mesh.calcRadius();

  atom_mesh.loadFromObjFile(
      getConfig()["AssetDirectoryFilepath"].get<std::string>() + getConfig()["SphereMeshFilepath"].get<std::string>());
  atom_mesh.optimizeMesh();
  atom_mesh.calcRadius();

  cylinder_mesh.loadFromObjFile(getConfig()["AssetDirectoryFilepath"].get<std::string>()
                                    + getConfig()["CylinderMeshFilepath"].get<std::string>());
  cylinder_mesh.optimizeMesh();
  cylinder_mesh.calcRadius();

  vector_mesh.loadFromObjFile(
      getConfig()["AssetDirectoryFilepath"].get<std::string>() + getConfig()["VectorMeshFilepath"].get<std::string>());
  vector_mesh.optimizeMesh();
  vector_mesh.calcRadius(); //BUGFIX!!

  bond_mesh.loadFromObjFile(
      getConfig()["AssetDirectoryFilepath"].get<std::string>() + getConfig()["BondMeshFilepath"].get<std::string>());
  bond_mesh.calcRadius();
  bond_mesh.optimizeMesh();

  meshes = MeshMerger();
  // load meshes and free cpu side mesh data
  meshes.addMesh(atom_mesh, meshID::eAtom, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
      .addMesh(unit_cell_mesh, meshID::eUnitCell, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
      .addMesh(vector_mesh, meshID::eVector, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
      .addMesh(cylinder_mesh, meshID::eCylinder, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
      .addMesh(bond_mesh, meshID::eBond, bond_pipeline_->pipeline(), graphics_pipeline_layout_);
  uploadMesh(*meshes.accumulated_mesh_, meshes.indexBuffer, meshes.vertexBuffer);
  meshes.accumulated_mesh_.reset(nullptr);

  scene_->setMeshes(&meshes);
}

void Engine::uploadMesh(Mesh &mesh, AllocatedBuffer &indexBuffer, AllocatedBuffer &vertexBuffer) {
  size_t vertex_buffer_size = mesh.vertices_.size()*sizeof(BasicVertex);

  //create staging buffer
  VkBufferCreateInfo vertex_staging_buffer_create_info = {};
  vertex_staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertex_staging_buffer_create_info.size = vertex_buffer_size;
  vertex_staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo vma_alloc_vertex_staging_buffer_info{};
  vma_alloc_vertex_staging_buffer_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  AllocatedBuffer vertexStagingBuffer{};

  vmaCreateBuffer(allocator_, &vertex_staging_buffer_create_info, &vma_alloc_vertex_staging_buffer_info,
                  &vertexStagingBuffer.buffer, &vertexStagingBuffer.allocation, nullptr);

  // writing to staging buffer
  void *vertexData;
  vmaMapMemory(allocator_, vertexStagingBuffer.allocation, &vertexData);
  memcpy(vertexData, mesh.vertices_.data(), vertex_buffer_size);
  vmaUnmapMemory(allocator_, vertexStagingBuffer.allocation);


  // create vertex buffer
  VkBufferCreateInfo vertex_buffer_create_info = {};
  vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertex_buffer_create_info.size = vertex_buffer_size;
  vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo vma_alloc_vertex_buffer_info{};
  vma_alloc_vertex_buffer_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  vmaCreateBuffer(allocator_, &vertex_buffer_create_info, &vma_alloc_vertex_buffer_info,
                  &vertexBuffer.buffer,
                  &vertexBuffer.allocation,
                  nullptr);

  // copy from staging to
  immediateSubmit([=](vk::CommandBuffer cmd) {
    vk::BufferCopy copy;
    copy.dstOffset = 0;
    copy.srcOffset = 0;
    copy.size = vertex_buffer_size;
    cmd.copyBuffer(vertexStagingBuffer.buffer, vertexBuffer.buffer, 1, &copy);
  });

  vmaDestroyBuffer(allocator_, vertexStagingBuffer.buffer, vertexStagingBuffer.allocation);

  main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Mesh Vertex Buffer" << "\n";
#endif
    vmaDestroyBuffer(allocator_, vertexBuffer.buffer, vertexBuffer.allocation);
  });


  //upload index buffer
  size_t index_buffer_size = mesh.indices_.size()*sizeof(uint32_t);

  //create staging buffer
  VkBufferCreateInfo index_staging_buffer_create_info = {};
  index_staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_staging_buffer_create_info.size = index_buffer_size;
  index_staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo vma_alloc_index_staging_buffer_info{};
  vma_alloc_index_staging_buffer_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  AllocatedBuffer indexStagingBuffer{};

  vmaCreateBuffer(allocator_, &index_staging_buffer_create_info, &vma_alloc_index_staging_buffer_info,
                  &indexStagingBuffer.buffer, &indexStagingBuffer.allocation, nullptr);

  // writing to staging buffer
  void *indexBufferData;
  vmaMapMemory(allocator_, indexStagingBuffer.allocation, &indexBufferData);
  memcpy(indexBufferData, mesh.indices_.data(), index_buffer_size);
  vmaUnmapMemory(allocator_, indexStagingBuffer.allocation);

  // create index buffer
  VkBufferCreateInfo index_buffer_create_info = {};
  index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_create_info.size = index_buffer_size;
  index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo vma_alloc_index_buffer_info{};
  vma_alloc_index_buffer_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  vmaCreateBuffer(allocator_, &index_buffer_create_info, &vma_alloc_index_buffer_info,
                  &indexBuffer.buffer,
                  &indexBuffer.allocation,
                  nullptr);

  // copy from staging to
  immediateSubmit([=](vk::CommandBuffer cmd) {
    vk::BufferCopy copy;
    copy.dstOffset = 0;
    copy.srcOffset = 0;
    copy.size = index_buffer_size;
    cmd.copyBuffer(indexStagingBuffer.buffer, indexBuffer.buffer, 1, &copy);
  });

  vmaDestroyBuffer(allocator_, indexStagingBuffer.buffer, indexStagingBuffer.allocation);

  main_destruction_stack_.push([=]() {

#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Mesh Index Buffer" << "\n";
#endif
    vmaDestroyBuffer(allocator_, indexBuffer.buffer, indexBuffer.allocation);
  });
}

FrameData &Engine::getCurrentFrame() {
  return frame_data_[framerate_control_.frame_number_%FRAMES_IN_FLIGHT];
}

const FrameData &Engine::getCurrentFrame() const {
  return frame_data_[framerate_control_.frame_number_%FRAMES_IN_FLIGHT];
}

uint32_t Engine::getCurrentFrameIndex() const {
  return framerate_control_.frame_number_%FRAMES_IN_FLIGHT;
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage) const {

  VkBufferCreateInfo buffer_create_info =
      static_cast<VkBufferCreateInfo>(vk::BufferCreateInfo{vk::BufferCreateFlags(), allocSize, usage});
  VmaAllocationCreateInfo allocation_create_info{};
  allocation_create_info.usage = memory_usage;

  AllocatedBuffer buffer{};
  vmaCreateBuffer(allocator_, &buffer_create_info, &allocation_create_info,
                  &buffer.buffer, &buffer.allocation, nullptr);
  return buffer;
}

void Engine::initDescriptors() {
  using buf = vk::BufferUsageFlagBits;

  descriptor_allocator_.init(logical_device_);
  layout_cache_.init(logical_device_);

  clear_draw_call_buffer_ =
      createBuffer(sizeof(GPUDrawCalls), buf::eTransferSrc | buf::eTransferDst, VMA_MEMORY_USAGE_GPU_ONLY);

  indirect_dispatch_buffer_ =
      createBuffer(sizeof(vk::DispatchIndirectCommand), buf::eIndirectBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

  const size_t sceneDataBufferSize = FRAMES_IN_FLIGHT*paddedUniformBufferSize(sizeof(GPUSceneData));
  scene_data_buffer_ =
      createBuffer(sceneDataBufferSize, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
  vk::DescriptorBufferInfo descriptorSceneBufferInfo{scene_data_buffer_.buffer, 0, sizeof(GPUSceneData)};

  for (auto &frame : frame_data_) {

    // 3 steps of binding a buffer
    // 1. Creation and 2. Description

    frame.mouseBucketBuffer =
        createBuffer(RCC_MOUSE_BUCKET_COUNT*sizeof(uint32_t), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo
        descriptorMouseBucketBufferInfo{frame.mouseBucketBuffer.buffer, 0, RCC_MOUSE_BUCKET_COUNT*sizeof(uint32_t)};

    frame.cam_buffer = createBuffer(sizeof(GPUCamData), buf::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo descriptorCamBufferInfo{frame.cam_buffer.buffer, 0, sizeof(GPUCamData)};

    frame.object_buffer =
        createBuffer((sizeof(GPUObjectData))*MAX_UNIQUE_OBJECTS, buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo
        descriptorObjectBufferInfo{frame.object_buffer.buffer, 0, sizeof(GPUObjectData)*MAX_UNIQUE_OBJECTS};

    frame.cull_data_buffer = createBuffer(sizeof(GPUCullData), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo descriptorCullDataBufferInfo{frame.cull_data_buffer.buffer, 0, sizeof(GPUCullData)};

    frame.offset_buffer = createBuffer(sizeof(GPUOffsets), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo descriptorOffsetBufferInfo{frame.offset_buffer.buffer, 0, sizeof(GPUOffsets)};

    frame.instance_buffer =
        createBuffer(sizeof(GPUInstance)*MAX_UNIQUE_OBJECTS, buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vk::DescriptorBufferInfo
        descriptorInstanceBufferInfo{frame.instance_buffer.buffer, 0, sizeof(GPUInstance)*MAX_UNIQUE_OBJECTS};

    frame.final_instance_buffer =
        createBuffer(sizeof(GPUFinalInstance)*MAX_UNIQUE_OBJECTS*27, buf::eStorageBuffer, VMA_MEMORY_USAGE_GPU_ONLY);
    vk::DescriptorBufferInfo descriptorFinalInstanceBufferInfo
        {frame.final_instance_buffer.buffer, 0, sizeof(GPUFinalInstance)*MAX_UNIQUE_OBJECTS*27};

    frame.draw_call_buffer = createBuffer(sizeof(GPUDrawCalls),
                                          buf::eStorageBuffer | buf::eIndirectBuffer | buf::eTransferDst,
                                          VMA_MEMORY_USAGE_GPU_ONLY);
    vk::DescriptorBufferInfo descriptorDrawCallBufferInfo{frame.draw_call_buffer.buffer, 0, sizeof(GPUDrawCalls)};

    // 3. Bondage
    DescriptorBuilder::begin(&layout_cache_, &descriptor_allocator_)
        .bindBuffer(0,
                    &descriptorCamBufferInfo,
                    vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .bindBuffer(1,
                    &descriptorSceneBufferInfo,
                    vk::DescriptorType::eUniformBufferDynamic,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
        .bindBuffer(2,
                    &descriptorObjectBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eVertex)
        .bindBuffer(3,
                    &descriptorMouseBucketBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eFragment)
        .bindBuffer(4,
                    &descriptorFinalInstanceBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eVertex)
        .bindBuffer(5,
                    &descriptorOffsetBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eVertex)
        .build(frame.globalDescriptorSet, graphics_descriptor_set_layout);

    DescriptorBuilder::begin(&layout_cache_, &descriptor_allocator_)
        .bindBuffer(0,
                    &descriptorObjectBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .bindBuffer(1,
                    &descriptorCullDataBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .bindBuffer(2,
                    &descriptorInstanceBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .bindBuffer(3,
                    &descriptorFinalInstanceBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .bindBuffer(4,
                    &descriptorDrawCallBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .bindBuffer(5,
                    &descriptorOffsetBufferInfo,
                    vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute)
        .build(frame.test_compute_shader_set, culling_descriptor_set_layout);


    // set mouseBucketBufferMemory to 0 when starting the program
    {
      void *mapped;
      vmaMapMemory(allocator_, frame.mouseBucketBuffer.allocation, &mapped);
      memset(mapped, 0, RCC_MOUSE_BUCKET_COUNT*sizeof(uint32_t));
      vmaUnmapMemory(allocator_, frame.mouseBucketBuffer.allocation);
    }

    // queue destroy buffer calls
    main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
      std::cout << "Destroying PerFrame Buffers" << "\n";
#endif
      vmaDestroyBuffer(allocator_, frame.cam_buffer.buffer, frame.cam_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.object_buffer.buffer, frame.object_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.cull_data_buffer.buffer, frame.cull_data_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.mouseBucketBuffer.buffer, frame.mouseBucketBuffer.allocation);
      vmaDestroyBuffer(allocator_, frame.offset_buffer.buffer, frame.offset_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.draw_call_buffer.buffer, frame.draw_call_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.instance_buffer.buffer, frame.instance_buffer.allocation);
      vmaDestroyBuffer(allocator_, frame.final_instance_buffer.buffer, frame.final_instance_buffer.allocation);
    });
  }

  main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying Descriptor Uniform Buffer" << "\n";
    std::cout << "Destroying ClearDrawBuffer" << "\n";
    std::cout << "Destroying IndirectDispatch" << "\n";
#endif
    vmaDestroyBuffer(allocator_, scene_data_buffer_.buffer, scene_data_buffer_.allocation);
    vmaDestroyBuffer(allocator_, clear_draw_call_buffer_.buffer, clear_draw_call_buffer_.allocation);
    vmaDestroyBuffer(allocator_, indirect_dispatch_buffer_.buffer, indirect_dispatch_buffer_.allocation);
  });

}

// https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
//returns the padded size of a struct in a uniform buffer
size_t Engine::paddedUniformBufferSize(size_t old_size) const {
  size_t min_ubo_alignment = gpu_properties_.limits.minUniformBufferOffsetAlignment;
  size_t aligned_size = old_size;
  if (min_ubo_alignment > 0) {
    aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
  }
  return aligned_size;
}

void Engine::beginRenderPass(vk::CommandBuffer &cmd, uint32_t swapchain_index) {

  vk::ClearValue clear_value;
  clear_value.color = {clearColor};
  vk::ClearValue depth_clear_value;
  depth_clear_value.depthStencil = 1.f;
  std::array<vk::ClearValue, 2> clearValues{clear_value, depth_clear_value};

  const vk::Extent2D window_extent{static_cast<uint32_t>(window_->width()), static_cast<uint32_t>(window_->height())};
  vk::RenderPassBeginInfo render_pass_begin_info
      {swapchain_->renderPass(), swapchain_->framebuffer(swapchain_index), vk::Rect2D{{0, 0}, window_extent},
       clearValues.size(),
       clearValues.data()};

  cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
  vk::Viewport
      viewport{0.f, 0.f, static_cast<float>(window_extent.width), static_cast<float>(window_extent.height), 0.f, 1.f};
  vk::Rect2D scissor{{0, 0}, window_extent};
  cmd.setViewport(0, viewport);
  cmd.setScissor(0, scissor);
}

void Engine::writeToBuffer(AllocatedBuffer &buffer, uint32_t range, void *data, uint32_t offset /* = 0*/) const {
  char *mapped;

  vmaMapMemory(allocator_, buffer.allocation, (void **) &mapped);
  mapped += offset;
  memcpy(mapped, data, range);
  vmaUnmapMemory(allocator_, buffer.allocation);
}

void Engine::readFromBufferAndClearIt(AllocatedBuffer &buffer,
                                      uint32_t range,
                                      void *data,
                                      uint32_t offset /* = 0*/) const {
  char *mapped;

  vmaMapMemory(allocator_, buffer.allocation, (void **) &mapped);
  mapped += offset;
  memcpy(data, mapped, range);
  memset(mapped, 0, range);
  vmaUnmapMemory(allocator_, buffer.allocation);
}

void Engine::readFromBuffer(AllocatedBuffer &buffer, uint32_t range, void *data, uint32_t offset /* = 0*/) const {
  char *mapped;

  vmaMapMemory(allocator_, buffer.allocation, (void **) &mapped);
  mapped += offset;
  memcpy(data, mapped, range);
  vmaUnmapMemory(allocator_, buffer.allocation);
}

void Engine::immediateSubmit(std::function<void(vk::CommandBuffer cmd)> &&function) {
  vk::CommandBuffer cmd = upload_context_.command_buffer;
  vk::CommandBufferBeginInfo cmd_begin_info{vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr};

  cmd.begin(cmd_begin_info);
  function(cmd);
  cmd.end();
  vk::SubmitInfo submit_info{0, nullptr, nullptr, 1, &cmd, 0, nullptr};

  graphics_queue_.submit(submit_info, upload_context_.upload_fence);
  const auto fence_wait_result = logical_device_.waitForFences(upload_context_.upload_fence, true, 10'000'000'000);
  if (fence_wait_result!=vk::Result::eSuccess) abort();
  logical_device_.resetFences(upload_context_.upload_fence);
  logical_device_.resetCommandPool(upload_context_.command_pool);
}

void Engine::initVisData() {
  scene_ = std::make_unique<Scene>(db_filepath_, getConfig()["ExperimentID"]);
}

void Engine::toggleCameraMode() {
  camera_->is_isometric = !camera_->is_isometric;
}

glm::vec3 Engine::getCenterCoords() {
  if (!scene_->visManager) return glm::vec3{0.f};
  if (scene_->visManager->data().activeEvent) return scene_->visManager->data().activeEvent->center;

  int xN = scene_->gConfig.xCellCount;
  int yN = scene_->gConfig.yCellCount;
  int zN = scene_->gConfig.zCellCount;
  glm::vec3 center{0.f};
  glm::mat3 cellT = glm::transpose(scene_->visManager->data().unitCellGLM);
  center[0] = (xN%2!=0) ? glm::dot(cellT[0], glm::vec3(0.5f)) : 0.f;
  center[1] = (yN%2!=0) ? glm::dot(cellT[1], glm::vec3(0.5f)) : 0.f;
  center[2] = (zN%2!=0) ? glm::dot(cellT[2], glm::vec3(0.5f)) : 0.f;
  return center;
}

void glfw_mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
  Engine::keyboardBackedEngine->mouseButtonCallback(button, action, mods);
}

void Engine::mouseButtonCallback(int button, int action, int mods) {
  if (ui->wantMouse()) return;

  if (button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS) {
    bReadMousePickingBuffer_ = (mods | GLFW_MOD_SHIFT)==mods;
    bDragRotateCam_ = (mods | GLFW_MOD_CONTROL)==mods;
    //for no modifiers the selection rectangle is drawn
  }

  if (button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_RELEASE) {
    bReadMousePickingBuffer_ = false;
    bDragRotateCam_ = false;
  }

}

void glfw_scroll_callback(GLFWwindow *, double /*x_offset*/, double y_offset) {
  Engine::keyboardBackedEngine->scrollCallback(y_offset);
}

void Engine::scrollCallback(double y_offset) {
  if (ui->wantMouse()) return;

  const auto zoom_speed = camera_->isometric_view_settings_.zoom_speed;
  if (camera_->is_isometric) {
    auto &isometric_height = camera_->isometric_view_settings_.isometric_height;
    isometric_height -= static_cast<float>(y_offset)*zoom_speed;
    isometric_height = std::max(isometric_height, 0.0f);
  } else {
    auto &fovY = camera_->perspective_view_settings_.perspective_fovy;
    fovY -= static_cast<float>(y_offset)*zoom_speed;
  }
}

void glfw_key_callback(GLFWwindow * /*window*/, int key, int scancode, int action, int mods) {
  Engine::keyboardBackedEngine->keyCallback(key, scancode, action, mods);
}


void Engine::cleanupMeasurementMode() {
  scene_->visManager->removeSelectedForMeasurementTags();
  selected_atom_numbers_.clear();
}

void Engine::cleanupSelectAndTagMode() {
  scene_->visManager->removeSelectedByAreaTags();
}

void Engine::keyCallback(int key, int /*scancode*/, int action, int /*mods*/) {
  if (ui->wantKeyboard()) return;
  if (key==GLFW_KEY_ESCAPE && action==GLFW_PRESS) {
    if(ui_mode_ == uiMode::eSelectAndTag) {cleanupSelectAndTagMode();}
    if(ui_mode_ == uiMode::eMeasure) {cleanupMeasurementMode();}
  }
  if (key==GLFW_KEY_SPACE && action==GLFW_PRESS) toggleFrameControlMode();
  if (key==GLFW_KEY_TAB && action==GLFW_PRESS) toggleCameraMode();
}

void Engine::initGlfwCallbacks() {
  glfwSetMouseButtonCallback(window_->glfwWindow_, glfw_mouse_button_callback);
  glfwSetKeyCallback(window_->glfwWindow_, glfw_key_callback);
  glfwSetScrollCallback(window_->glfwWindow_, glfw_scroll_callback);
}

void Engine::writeCameraBuffer() {
  camera_->system_center = getCenterCoords();

  //use the average frame time for camera updates to avoid camera jumps on lag frames
  if (!ui->wantKeyboard()) {
    camera_->UpdateCamera(framerate_control_.avgFrameTime.avg(), window_->glfwWindow_);
  }
  auto cylinderType = reinterpret_cast<CylinderType *>(&(*scene_)["Cylinder"]);
  cylinderType->camera_view_direction = camera_->view_direction_;

  GPUCamData ubo{};
  const vk::Extent2D window_extent{static_cast<uint32_t>(window_->width()), static_cast<uint32_t>(window_->height())};
  ubo.viewMat = camera_->GetViewMatrix();
  ubo.projViewMat = camera_->GetProjectionMatrix(window_extent)*ubo.viewMat;
  ubo.cam_position = (camera_->is_isometric) ?
                     glm::vec4(camera_->position_
                                   - camera_->view_direction_*camera_->isometric_view_settings_.isometric_depth, 1.f)
                                             : glm::vec4(camera_->GetPosition(), 1.f);
  ubo.direction_of_light = glm::vec4(-camera_->view_direction_, 1.f);//glm::vec4(camera_->GetUp(), 1.f);

  writeToBuffer(getCurrentFrame().cam_buffer, sizeof(GPUCamData), &ubo);
}

void Engine::writeSceneBuffer() {
  //write mouse coords to push constant
  double mouse_coords[2] = {0, 0}; // for float to double conversion
  glfwGetCursorPos(window_->glfwWindow_, &mouse_coords[0], &mouse_coords[1]);
  scene_data_.mouseCoords[0] = static_cast<float>(mouse_coords[0]);
  scene_data_.mouseCoords[1] = static_cast<float>(mouse_coords[1]);
  scene_data_.pointLights[0].position = glm::vec4(camera_->GetPosition(), 1.f);
  scene_data_.pointLights[0].lightColor = glm::vec4(1.f, 1.f, 1.f, 50.f);
  scene_data_.ambientColor = glm::vec4(1.f, 1.f, 1.f, 0.02f);

  uint32_t gpu_ubo_offset = paddedUniformBufferSize(sizeof(GPUSceneData))*getCurrentFrameIndex();
  writeToBuffer(scene_data_buffer_, sizeof(GPUSceneData), &scene_data_, gpu_ubo_offset);
}

void Engine::writeCullBuffer() {

  const vk::Extent2D window_extent{static_cast<uint32_t>(window_->width()), static_cast<uint32_t>(window_->height())};

  glm::mat4 projectionT = glm::transpose(camera_->GetProjectionMatrix(window_extent));

  GPUCullData cullData = {};

  cullData.viewMatrix = camera_->GetViewMatrix();

  //normal culling
  cullData.frustumNormalEquations[0] = normalizePlane(projectionT[3] + projectionT[0]);
  cullData.frustumNormalEquations[1] = normalizePlane(projectionT[3] - projectionT[0]);
  cullData.frustumNormalEquations[2] = normalizePlane(projectionT[3] + projectionT[1]);
  cullData.frustumNormalEquations[3] = normalizePlane(projectionT[3] - projectionT[1]);
  cullData.frustumNormalEquations[4] = normalizePlane(projectionT[3] + projectionT[2]);
  cullData.frustumNormalEquations[5] = normalizePlane(projectionT[3] - projectionT[2]);
  cullData.uniqueObjectCount = scene_->uniqueShownObjectCount(GetMovieFrameIndex());
  cullData.offsetCount = scene_->gConfig.xCellCount*scene_->gConfig.yCellCount*scene_->gConfig.zCellCount;
  cullData.isCullingEnabled = isCullingEnabled;

  //cylinder culling
  if (scene_->visManager->data().activeEvent!=nullptr) {
    cullData.cullCylinder = scene_->eventViewerSettings.enableCylinderCulling;
    cullData.cylinderCenter = glm::vec4(scene_->visManager->data().activeEvent->center, 0);
    cullData.cylinderNormal =
        glm::vec4(scene_->eventViewerSettings.surfaceNormals ? scene_->visManager->data().activeEvent->surfaceNormal
                                                             : scene_->visManager->data().activeEvent->connectionNormal,
                  0);
    cullData.cylinderLength = scene_->eventViewerSettings.cylinderLength;
    cullData.cylinderRadiusSquared =
        scene_->eventViewerSettings.cylinderRadius*scene_->eventViewerSettings.cylinderRadius;
  } else {
    cullData.cullCylinder = false;
  }

  writeToBuffer(getCurrentFrame().cull_data_buffer, sizeof(GPUCullData), &cullData);
}

void Engine::writeClearDrawCallBuffer() {

  GPUDrawCalls draws;
  uint32_t previousFirstInstance = 0;
  uint32_t previousMaxObjectCount = 0;
  for (const auto &type : scene_->objectTypes) {
    draws.commands[type->mesh_id].indexCount = meshes.meshInfos[type->mesh_id].indexCount;
    draws.commands[type->mesh_id].firstIndex = meshes.meshInfos[type->mesh_id].firstIndex;
    draws.commands[type->mesh_id].vertexOffset = meshes.meshInfos[type->mesh_id].firstVertex;
    draws.commands[type->mesh_id].instanceCount = 0;
    draws.commands[type->mesh_id].firstInstance = previousFirstInstance + 27*previousMaxObjectCount;

    previousFirstInstance = draws.commands[type->mesh_id].firstInstance;
    previousMaxObjectCount = type->MaxCount();
  }

  stageBuffer(&draws, clear_draw_call_buffer_, sizeof(draws), 0);
}

GPUOffsets Engine::getOffsets() {
  GPUOffsets gpu_offsets = {};
  const auto &glm_basis = scene_->cellGLM();

  int &xN = scene_->gConfig.xCellCount;
  int &yN = scene_->gConfig.yCellCount;
  int &zN = scene_->gConfig.zCellCount;

  xN = std::max(1, xN);
  yN = std::max(1, yN);
  zN = std::max(1, zN);
  xN = std::min(3, xN);
  yN = std::min(3, yN);
  zN = std::min(3, zN);

  //xN = std::clamp(xN, 1, max_cell_count_ / (yN*zN));
  //yN = std::clamp(yN, 1, max_cell_count_ / (xN*zN));
  //zN = std::clamp(zN, 1, max_cell_count_ / (yN*xN));

  assert(xN*yN*zN <= max_cell_count_);

  int index = 0;
  for (int i = 0; i < xN; i++) {
    for (int j = 0; j < yN; j++) {
      for (int k = 0; k < zN; k++) {
        float signX = (i%2==0) ? 1.f : -1.f;
        float signY = (j%2==0) ? 1.f : -1.f;
        float signZ = (k%2==0) ? 1.f : -1.f;
        gpu_offsets.offsets[index] = glm::vec4(
            (signX*truncf(static_cast<float>(i + 1)/2.f))*glm_basis[0]
                + (signY*truncf(static_cast<float>(j + 1)/2.f))*glm_basis[1]
                + (signZ*truncf(static_cast<float>(k + 1)/2.f))*glm_basis[2], 0);
        index++;
      }
    }
  }
  return gpu_offsets;
}

void Engine::writeOffsetBuffer() {
  auto gpu_offsets = getOffsets();
  writeToBuffer(getCurrentFrame().offset_buffer, sizeof(GPUOffsets), &gpu_offsets);
}

void Engine::writeObjectAndInstanceBuffer() {

  void *objectData;
  vmaMapMemory(allocator_, getCurrentFrame().object_buffer.allocation, &objectData);
  auto *objectSSBO = (GPUObjectData *) objectData;

  void *instanceData;
  vmaMapMemory(allocator_, getCurrentFrame().instance_buffer.allocation, &instanceData);
  auto *instanceSSBO = (GPUInstance *) instanceData;

  scene_->writeObjectAndInstanceBuffer(objectSSBO, instanceSSBO, GetMovieFrameIndex(), selected_object_index_);

  vmaUnmapMemory(allocator_, getCurrentFrame().object_buffer.allocation);
  vmaUnmapMemory(allocator_, getCurrentFrame().instance_buffer.allocation);
}

nlohmann::json &Engine::getConfig() {
  static nlohmann::json config;
  return config;
}

vk::Pipeline Engine::createComputePipeline(const std::string &shader_path,
                                           const vk::PipelineLayout &compute_pipeline_layout) {

  vk::ShaderModule compute_shader_module =
      Pipeline::createShaderModule(logical_device_, Pipeline::readFile(shader_path));
  auto shader_stage_info = vk::PipelineShaderStageCreateInfo(
      vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, compute_shader_module, "main", nullptr);

  auto compute_pipeline_info =
      vk::ComputePipelineCreateInfo(vk::PipelineCreateFlags(), shader_stage_info, compute_pipeline_layout);
  auto result = logical_device_.createComputePipeline(nullptr, compute_pipeline_info);
  if (result.result!=vk::Result::eSuccess) {
    abort();
  }

  logical_device_.destroy(compute_shader_module);

  return result.value;
}

void Engine::initComputePipelines() {

  std::string cull_compute_module_path =
      getConfig()["AssetDirectoryFilepath"].get<std::string>() + getConfig()["CullShaderFilepath"].get<std::string>();

  auto compute_pipeline_layout_info = vk::PipelineLayoutCreateInfo{
      vk::PipelineLayoutCreateFlags(), culling_descriptor_set_layout, nullptr};
  culling_compute_pipeline_layout_ = logical_device_.createPipelineLayout(compute_pipeline_layout_info);
  culling_compute_pipeline_ = createComputePipeline(cull_compute_module_path, culling_compute_pipeline_layout_);

  main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying compute pipeline and layout\n";
#endif
    logical_device_.destroy(culling_compute_pipeline_);
    logical_device_.destroy(culling_compute_pipeline_layout_);
  });
}

void Engine::stageBuffer(void *src,
                         AllocatedBuffer dest,
                         vk::DeviceSize size,
                         vk::DeviceSize offset) {

  // staging buffer info
  using buf = vk::BufferUsageFlagBits;
  VmaAllocationCreateInfo sb_alloc_info = {};
  sb_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  // staging buffer creation
  AllocatedBuffer staging_buffer = createBuffer(size, buf::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);
  writeToBuffer(staging_buffer, size, src, offset);

  immediateSubmit([=](vk::CommandBuffer cmd) {
    vk::BufferCopy copy{0, offset, size};
    cmd.copyBuffer(staging_buffer.buffer, dest.buffer, 1, &copy);
  });
  vmaDestroyBuffer(allocator_, staging_buffer.buffer, staging_buffer.allocation);
}

void Engine::resetDrawData(vk::CommandBuffer &cmd,
                           AllocatedBuffer src,
                           AllocatedBuffer dst,
                           vk::DeviceSize size,
                           vk::DeviceSize src_offset,
                           vk::DeviceSize dst_offset) const {

  vk::BufferCopy copy{src_offset, dst_offset, size};
  cmd.copyBuffer(src.buffer, dst.buffer, copy);

  using acs = vk::AccessFlagBits;
  vk::BufferMemoryBarrier barrier
      {acs::eTransferWrite, acs::eShaderRead | acs::eShaderWrite, graphics_queue_family_, graphics_queue_family_,
       dst.buffer, dst_offset, size};
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                      vk::PipelineStageFlagBits::eComputeShader,
                      {},
                      nullptr,
                      barrier,
                      nullptr);
}

void Engine::writeIndirectDispatchBuffer() {
  uint32_t group_count = ceil(scene_->uniqueShownObjectCount(GetMovieFrameIndex())/256.0);
  vk::DispatchIndirectCommand command{group_count, 1, 1};

  writeToBuffer(indirect_dispatch_buffer_, sizeof(vk::DispatchIndirectCommand), &command);
}

void Engine::enterEventMode(int eventID) {
  //clean up old event if there is one
  if (scene_->visManager->data().activeEvent) leaveEventMode();
  scene_->visManager->loadActiveEvent(eventID);
  (*scene_)["UnitCell"].shown = false;
  (*scene_)["Cylinder"].shown = true;
  scene_->visManager->addEventTags(*scene_->visManager->data().activeEvent);
  GetOptimalCameraPerspective();
}

void Engine::GetOptimalCameraPerspective() {
  using std::cout, std::endl;
  const uint32_t spacingFrameCount = 80;
  const Event &event = *scene_->visManager->data().activeEvent;
  uint32_t firstFrameNumber =
      std::clamp(static_cast<uint32_t>(event.frameNumber - spacingFrameCount), 0U, scene_->MovieFrameCount() - 1);
  uint32_t lastFrameNumber =
      std::clamp(static_cast<uint32_t>(event.frameNumber + spacingFrameCount), 0U, scene_->MovieFrameCount() - 1);
  const uint32_t frame_count = lastFrameNumber - firstFrameNumber + 1;
  const glm::vec3 up{0.f, 0.f, 1.f};
  const glm::vec3 normal =
      glm::normalize((scene_->eventViewerSettings.surfaceNormals) ? event.surfaceNormal : event.connectionNormal);

  const glm::vec3 rotationAxis = glm::cross(normal, up);
  const float angle = glm::orientedAngle(normal, up, rotationAxis);

  //rotation matrix to a coordinate system where the normal (the hinuma vector) is equal to (0,0,1)
  glm::mat4 modelMatrixGLM = glm::rotate(glm::mat4{1.f}, angle, rotationAxis);
  modelMatrixGLM = glm::translate(modelMatrixGLM, -event.center);
  Eigen::Matrix4f modelMatrix;
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) modelMatrix(i, j) = modelMatrixGLM[i][j];

  // Grab relevant positions and transform them
  Eigen::MatrixX4f
      positions = Eigen::MatrixX4f::Zero(static_cast<uint32_t>(event.chemical_positions.size())*frame_count, 4);
  for (uint32_t i = firstFrameNumber; i <= lastFrameNumber; i++) {
    for (uint32_t j = 0; j < event.chemical_positions.size(); j++) {
      uint32_t index = (i - firstFrameNumber)*event.chemical_positions.size() + j;
      positions(index, 0) = scene_->visManager->data().positions[i](event.chemical_atom_numbers[j], 0);
      positions(index, 1) = scene_->visManager->data().positions[i](event.chemical_atom_numbers[j], 1);
      positions(index, 2) = scene_->visManager->data().positions[i](event.chemical_atom_numbers[j], 2);
      positions(index, 3) = 1;
    }
  }
  Eigen::MatrixX2f transformedPositions = (positions*modelMatrix).topLeftCorner(positions.rows(), 2);

  // calculate the center of the transformed positions and center them
  Eigen::Vector2f center = Eigen::Vector2f::Zero();
  for (int i = 0; i < transformedPositions.rows(); i++) {
    center += transformedPositions.row(i);
  }
  center /= static_cast<float>(transformedPositions.rows());
  for (int i = 0; i < transformedPositions.rows(); i++) {
    transformedPositions.row(i) -= center;
  }

  cout << transformedPositions << endl << endl << endl;
  Eigen::JacobiSVD<Eigen::MatrixXf, Eigen::ComputeThinU | Eigen::ComputeThinV> svd(transformedPositions);
  cout << "Its singular values are:" << endl << svd.singularValues() << endl;
  cout << "Its left singular vectors are the columns of the thin U matrix:" << endl << svd.matrixU() << endl;
  cout << "Its right singular vectors are the columns of the thin V matrix:" << endl << svd.matrixV() << endl;

  camera_->isometric_offset_ = {0.f, 0.f};
  camera_->up_direction_ = normal;
  glm::vec4 transformed_view_direction_ = glm::vec4(svd.matrixV()(0, 1), svd.matrixV()(1, 1), 0, 0);
  camera_->view_direction_ = glm::vec3(glm::inverse(modelMatrixGLM)*transformed_view_direction_);
  camera_->position_ = event.center - camera_->view_direction_*scene_->eventViewerSettings.cylinderLength;
}

void Engine::leaveEventMode() {
  scene_->visManager->removeEventTags(*scene_->visManager->data().activeEvent);
  (*scene_)["UnitCell"].shown = true;
  (*scene_)["Cylinder"].shown = false;
  scene_->visManager->unloadActiveEvent();
}

void Engine::DumpSettingsJson(const std::string &filepath) {
  //update the json values before dumping
  glm::vec4& col = scene_->gConfig.catalyst_color_;
  getConfig()["CatalystColor"] = std::array<float,4>{col.r, col.g, col.b, col.a};
  col = scene_->gConfig.chemical_color_;
  getConfig()["ChemicalColor"] = std::array<float,4>{col.r, col.g, col.b, col.a};
  getConfig()["MovieFrameRate"] = framerate_control_.movie_framerate_;
  getConfig()["UseLightMode"] = ui->bLightMode;
  getConfig()["DragSpeed"] = camera_->drag_speed_;
  getConfig()["UseIsometric"] = camera_->is_isometric;
  getConfig()["WindowWidth"] = window_->width();
  getConfig()["WindowHeight"] = window_->height();
  getConfig()["NearPlane"] = camera_->perspective_view_settings_.near;
  getConfig()["FarPlane"] = camera_->perspective_view_settings_.far;
  getConfig()["FOVY"] = camera_->perspective_view_settings_.perspective_fovy;
  getConfig()["IsIsometric"] = camera_->is_isometric;
  getConfig()["IsometricHeight"] = camera_->isometric_view_settings_.isometric_height;
  getConfig()["MovementSpeed"] = camera_->perspective_view_settings_.move_speed;
  getConfig()["TurnSpeed"] = camera_->perspective_view_settings_.turn_speed;
  getConfig()["ZoomSpeed"] = camera_->isometric_view_settings_.zoom_speed;
  getConfig()["IsometricDepth"] = camera_->isometric_view_settings_.isometric_depth;
  getConfig()["AtomSize"] = scene_->gConfig.atomSize;
  getConfig()["BondLength"] = scene_->gConfig.bondLength;
  getConfig()["BondThickness"] = scene_->gConfig.bondThickness;
  getConfig()["HinumaLength"] = scene_->gConfig.hinumaVectorLength;
  getConfig()["HinumaThickness"] = scene_->gConfig.hinumaVectorThickness;
  getConfig()["BoxCountX"] = scene_->gConfig.xCellCount;
  getConfig()["BoxCountY"] = scene_->gConfig.yCellCount;
  getConfig()["BoxCountZ"] = scene_->gConfig.zCellCount;
  getConfig()["ClearColor"] = clearColor;
  getConfig()["MaxCellCount"] = max_cell_count_;

  //dumb
  std::ofstream out_file(filepath);
  out_file << Engine::getConfig().dump(4) << std::endl;
}

glm::vec2 mapScreenToIso(glm::vec2 coords, float width, float height, float iso_width, float iso_height) {
  glm::vec2 mapped{0.f};
  mapped[0] = -iso_width + coords[0]*2.f*(iso_width/width);
  mapped[1] = iso_height - coords[1]*2.f*(iso_height/height);
  return mapped;
}

void Engine::selectAtomsWithRect(glm::vec2 start, glm::vec2 end, int frame_index) {
  if (!scene_->visManager) return;
  float iso_height = camera_->isometric_view_settings_.isometric_height;
  float iso_width = iso_height*static_cast<float>(window_->aspect());
  auto mapped1 = mapScreenToIso(start,
                                static_cast<float>(window_->width()),
                                static_cast<float>(window_->height()),
                                iso_width,
                                iso_height);
  auto mapped2 = mapScreenToIso(end,
                                static_cast<float>(window_->width()),
                                static_cast<float>(window_->height()),
                                iso_width,
                                iso_height);

  glm::mat4 proj = glm::ortho(std::min(mapped1[0], mapped2[0]) + camera_->isometric_offset_[0],
                              std::max(mapped1[0], mapped2[0]) + camera_->isometric_offset_[0],
                              std::min(mapped1[1], mapped2[1]) - camera_->isometric_offset_[1],
                              std::max(mapped1[1], mapped2[1]) - camera_->isometric_offset_[1],
                              -camera_->isometric_view_settings_.isometric_depth,
                              camera_->isometric_view_settings_.isometric_depth);
  //proj[1][1] *= -1;
  glm::mat4 projT = glm::transpose(proj);

  //normal culling
  std::array<glm::vec4, 6> frustumNormalEquations{};
  frustumNormalEquations[0] = normalizePlane(projT[3] + projT[0]);
  frustumNormalEquations[1] = normalizePlane(projT[3] - projT[0]);
  frustumNormalEquations[2] = normalizePlane(projT[3] + projT[1]);
  frustumNormalEquations[3] = normalizePlane(projT[3] - projT[1]);
  frustumNormalEquations[4] = normalizePlane(projT[3] + projT[2]);
  frustumNormalEquations[5] = normalizePlane(projT[3] - projT[2]);

  // cpu culling :(((
  const Eigen::MatrixX3f &positions = scene_->visManager->data().positions[frame_index];
  GPUOffsets mic_offsets = getOffsets();

  for (int i = 0; i < scene_->visManager->data().positions[frame_index].rows(); i++) {
    // W = World Space, C = Camera Space
    glm::vec4 position_world = glm::vec4(positions(i, 0), positions(i, 1), positions(i, 2), 1);

    float element_radius = scene_->visManager->data().elementInfos.find(scene_->visManager->data().tags(i) & 255)->second.atomRadius;
    float radius = scene_->meshes->meshInfos[meshID::eAtom].radius*element_radius*scene_->gConfig.atomSize;

    bool insideRect;

    //is atom i in the selection for any of its mic super positions
    for (int j = 0; j < scene_->gConfig.xCellCount*scene_->gConfig.yCellCount*scene_->gConfig.zCellCount; j++) {
      insideRect = true;
      glm::vec4 position_cam = camera_->GetViewMatrix()*(position_world + mic_offsets.offsets[j]);
      for (int k = 0; k < 6; k++) {
        insideRect = insideRect && (glm::dot(frustumNormalEquations[k], position_cam) > -radius); //
      }
      if (insideRect) break;
    }

    if (insideRect) {
      scene_->visManager->getTagsRef()(i) |= Tags::eSelectedForTagging;
    }
  }
}

} //namespace rcc