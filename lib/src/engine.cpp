#include "engine.hpp"
#include "gui.hpp"
#include "buffer.hpp"
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

Engine::Engine(const char *db_filepath, const char *asset_dir_path) {
    //needed to get feedback from inside glfw callback functions
    Engine::keyboardBackedEngine = this;

    // if a directory path was given overwrite the default one
    if (asset_dir_path) asset_dir_filepath_ = std::string(asset_dir_path);
    if (db_filepath) db_filepath_ = std::string(db_filepath);

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
    resource_manager_ = std::make_unique<ResourceManager>(logical_device_, allocator_);
    initCamera();
    recreateSwapchain();
    initCommands();
    initSyncStructures();
    initDescriptors();
    initPipelines();
    scene_ = std::make_unique<Scene>();
    initScene();
    initComputePipelines();
    ui = std::make_unique<UserInterface>(this);
    if (!db_filepath_.empty()) connectToDB();
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
        .require_api_version(1, 2, 0)
        .request_validation_layers(enableValidationLayers)
        //.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
        //.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
        //.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
        .use_default_debug_messenger()
        .build()
        .value();

    instance_ = vkbInstance.instance;
    debug_messenger_ = vkbInstance.debug_messenger;

    window_->createSurface(instance_, &surface_);

    VkPhysicalDeviceFeatures required_device_features{};
    required_device_features.samplerAnisotropy = VK_TRUE;
    required_device_features.fragmentStoresAndAtomics = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice vkbPhysicalDevice = selector
        .set_minimum_version(1, 1)
        .set_surface(surface_)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .set_required_features(required_device_features)
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
            bool wasSelectedBefore =
                scene_->visManager->getTagsRef()(selected_object_index_) & Tags::eSelectedForMeasurement;
            scene_->visManager->getTagsRef()(selected_object_index_) |= Tags::eSelectedForMeasurement;

            if (!wasSelectedBefore) selected_atom_numbers_.push_back(selected_object_index_);

            if (selected_atom_numbers_.size() > 3) {
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
        glm::mat3
            rightDirectionRotationMat = glm::mat3(glm::rotate(-static_cast<float>(offset[1]*dx), right_direction));
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
        long int frame_index = ((framerate_control_.frame_number_ - 1)%Swapchain::MAX_FRAMES_IN_FLIGHT);
        if (frame_index==-1) frame_index = Swapchain::MAX_FRAMES_IN_FLIGHT - 1;

        resource_manager_->readFromBufferAndClearIt(
            frame_data_[frame_index].mouse_bucket_buffer,
            sizeof(uint32_t)*RCC_MOUSE_BUCKET_COUNT,
            mouse_buckets);

        if (experiment_state_!=State::eNone) {
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

        if (scene_->visManager) {
            if (window_->windowName()
                !=(getConfig()["WindowName"].get<std::string>()) + " - " + scene_->visManager->getDBFilepath()) {
                window_->setWindowName(
                    (getConfig()["WindowName"].get<std::string>()) + " - " + scene_->visManager->getDBFilepath());
            }
        } else {
            if (window_->windowName()!=(getConfig()["WindowName"].get<std::string>())) {
                window_->setWindowName((getConfig()["WindowName"].get<std::string>()));
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
            cmdBufferAllocateInfo =
            vk::CommandBufferAllocateInfo(frame.command_pool, vk::CommandBufferLevel::ePrimary, 1);

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
        frame.offscreen_semaphore = logical_device_.createSemaphore(semaphore_create_info);
        main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
            std::cout << "Destroying Present and Render Semaphores and Render Fence" << "\n";
#endif
            logical_device_.destroy(frame.present_semaphore);
            logical_device_.destroy(frame.render_semaphore);
            logical_device_.destroy(frame.offscreen_semaphore);
            logical_device_.destroy(frame.render_fence);
        });
    }
}

void Engine::render() {
    ui->render();

    auto new_time = std::chrono::high_resolution_clock::now();
    framerate_control_.frame_time_ =
        static_cast<float>(std::chrono::duration<double, std::chrono::milliseconds::period>(
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

    if (experiment_state_!=eNone) {
        //reset IndirectDrawClearBuffer if we have a new Experiment

        if (experiment_state_==State::eNew) {
            loadMeshes();
            writeClearDrawCallBuffer();
            experiment_state_ = State::eOld;
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

    if (experiment_state_!=eNone) {
        resetDrawData(cmd, clear_draw_call_buffer_, getCurrentFrame().draw_call_buffer, sizeof(GPUDrawCalls));
        runCullComputeShader(cmd);
    }

    beginOffscreenRenderPass(cmd);
    if (experiment_state_ != eNone) draw(cmd);
    cmd.endRenderPass();

    beginPresentRenderPass(cmd, swapchainIndex);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, deferred_pipeline_layout_, 0, getCurrentFrame().deferred_descriptor_set, nullptr);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, deferred_pipeline_->pipeline());
    cmd.draw(3, 1, 0, 0);
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

void Engine::draw(vk::CommandBuffer &cmd) {
    //bind Global Descriptor Set and Vertex-/Index-Buffers
    uint32_t gpu_ubo_offset = paddedUniformBufferSize(sizeof(GPUSceneData))*getCurrentFrameIndex();

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_layout_,
                           0, 1, &getCurrentFrame().global_descriptor_set,
                           1, &gpu_ubo_offset);

    auto &vertex_buffer = resource_manager_->getBuffer(meshes.accumulated_mesh_->vertexBuffer_.handle_);
    auto &index_buffer = resource_manager_->getBuffer(meshes.accumulated_mesh_->indexBuffer_.handle_);

    cmd.bindVertexBuffers(0, vertex_buffer.buffer_, {0});
    cmd.bindIndexBuffer(index_buffer.buffer_, 0, vk::IndexType::eUint32);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, atom_pipeline_->pipeline());

    if ((*scene_)["Atom"].isLoaded() && (*scene_)["Atom"].shown) {

        //auto indices = meshes.meshInfos[meshID::eAtom].indexCount;
        //cmd.drawIndexed(indices, 1400, 0, 0, 0);

        cmd.drawIndexedIndirect(resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_,
                                sizeof(vk::DrawIndexedIndirectCommand)*meshID::eAtom,
                                1,
                                sizeof(vk::DrawIndexedIndirectCommand));

    }

    if ((*scene_)["UnitCell"].isLoaded() && (*scene_)["UnitCell"].shown) {
        cmd.drawIndexedIndirect(resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_,
                                sizeof(vk::DrawIndexedIndirectCommand)*meshID::eUnitCell,
                                1,
                                sizeof(vk::DrawIndexedIndirectCommand));
    }

    if ((*scene_)["Vector"].isLoaded() && (*scene_)["Vector"].shown) {
        cmd.drawIndexedIndirect(resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_,
                                sizeof(vk::DrawIndexedIndirectCommand)*meshID::eVector,
                                1,
                                sizeof(vk::DrawIndexedIndirectCommand));
    }

    if ((*scene_)["Cylinder"].isLoaded() && (*scene_)["Cylinder"].shown) {
        cmd.drawIndexedIndirect(resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_,
                                sizeof(vk::DrawIndexedIndirectCommand)*meshID::eCylinder,
                                1,
                                sizeof(vk::DrawIndexedIndirectCommand));
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, bond_pipeline_->pipeline());

    if ((*scene_)["Bond"].isLoaded() && (*scene_)["Bond"].shown) {
        cmd.drawIndexedIndirect(resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_,
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
                           getCurrentFrame().compute_shader_set,
                           {});

    cmd.dispatchIndirect(resource_manager_->getBuffer(indirect_dispatch_buffer_.handle_).buffer_, 0);

    std::vector<vk::BufferMemoryBarrier> barriers = {
        vk::BufferMemoryBarrier{vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead,
                                graphics_queue_family_, graphics_queue_family_,
                                resource_manager_->getBuffer(getCurrentFrame().final_instance_buffer.handle_).buffer_,
                                0,
                                MAX_UNIQUE_OBJECTS*sizeof(GPUFinalInstance)*27},
        vk::BufferMemoryBarrier{vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
                                vk::AccessFlagBits::eMemoryRead,
                                graphics_queue_family_, graphics_queue_family_,
                                resource_manager_->getBuffer(getCurrentFrame().draw_call_buffer.handle_).buffer_, 0,
                                sizeof(GPUDrawCalls)}
    };

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader,
                        vk::DependencyFlags(), nullptr, barriers, nullptr);
}

void Engine::cleanup() {
    logical_device_.waitIdle();
    resource_manager_.reset(nullptr);
    swapchain_.reset();
    main_destruction_stack_.flush();
    descriptor_allocator_.cleanup();
    layout_cache_.cleanup();

    atom_pipeline_.reset(nullptr);
    bond_pipeline_.reset(nullptr);
    deferred_pipeline_.reset(nullptr);

    instance_.destroy(surface_);
    logical_device_.destroy();
    vkb::destroy_debug_utils_messenger(instance_, debug_messenger_);
    instance_.destroy();
}

void Engine::initPipelines() {
    // Create Pipeline Layouts
    vk::PipelineLayoutCreateInfo atom_bond_pipeline_layout_info{vk::PipelineLayoutCreateFlags(), graphics_descriptor_set_layout};
    graphics_pipeline_layout_ = logical_device_.createPipelineLayout(atom_bond_pipeline_layout_info);

    vk::PipelineLayoutCreateInfo deferred_pipeline_layout_info{vk::PipelineLayoutCreateFlags(), deferred_descriptor_set_layout};
    deferred_pipeline_layout_ = logical_device_.createPipelineLayout(deferred_pipeline_layout_info);

    main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
        std::cout << "Destroying Graphics Pipeline Layouts" << "\n";
#endif
        logical_device_.destroy(graphics_pipeline_layout_);
        logical_device_.destroy(deferred_pipeline_layout_);
    });

    //get shader paths;
    const std::string atom_vs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["AtomVertexShaderFilepath"].get<std::string>();
    const std::string atom_fs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["AtomFragmentShaderFilepath"].get<std::string>();
    const std::string bond_vs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["BondVertexShaderFilepath"].get<std::string>();
    const std::string bond_fs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["BondFragmentShaderFilepath"].get<std::string>();
    const std::string deferred_vs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["DeferredVertexShaderFilepath"].get<std::string>();
    const std::string deferred_fs_path = getConfig()["AssetDirectoryFilepath"].get<std::string>()
        + getConfig()["DeferredFragmentShaderFilepath"].get<std::string>();


    PipelineConfig config{};
    Pipeline::defaultPipelineConfigInfo(config);
    config.pipelineLayout = graphics_pipeline_layout_;

    // offscreen pipelines
    config.renderPass = swapchain_->offscreen_framebuffer.renderPass;

    VertexDescription vertexDescription = BasicVertex::getDescription();
    config.bindingDescriptions = vertexDescription.bindings_;
    config.attributeDescriptions = vertexDescription.attributes_;


    config.colorBlendAttachment = std::vector<vk::PipelineColorBlendAttachmentState>(3, vk::PipelineColorBlendAttachmentState{false});
    config.colorBlendInfo = vk::PipelineColorBlendStateCreateInfo{
        vk::PipelineColorBlendStateCreateFlags(), false,
        vk::LogicOp::eCopy, config.colorBlendAttachment};

    atom_pipeline_ = std::make_unique<Pipeline>(logical_device_, config, atom_vs_path, atom_fs_path);
    bond_pipeline_ = std::make_unique<Pipeline>(logical_device_, config, bond_vs_path, bond_fs_path);

    // onscreen pipeline
    config.pipelineLayout = deferred_pipeline_layout_;
    config.renderPass = swapchain_->renderPass();
    // we have no real vertices
    config.bindingDescriptions.clear();
    config.attributeDescriptions.clear();
    // pls no culling

    config.colorBlendAttachment = std::vector<vk::PipelineColorBlendAttachmentState>(1, vk::PipelineColorBlendAttachmentState{false});
    config.colorBlendInfo = vk::PipelineColorBlendStateCreateInfo{
        vk::PipelineColorBlendStateCreateFlags(), false,
        vk::LogicOp::eCopy, config.colorBlendAttachment};
    config.rasterizationInfo.cullMode = vk::CullModeFlagBits::eNone;

    deferred_pipeline_ = std::make_unique<Pipeline>(logical_device_, config, deferred_vs_path, deferred_fs_path);

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
        getConfig()["AssetDirectoryFilepath"].get<std::string>()
            + getConfig()["SphereMeshFilepath"].get<std::string>());
    atom_mesh.optimizeMesh();
    atom_mesh.calcRadius();

    cylinder_mesh.loadFromObjFile(getConfig()["AssetDirectoryFilepath"].get<std::string>()
                                      + getConfig()["CylinderMeshFilepath"].get<std::string>());
    cylinder_mesh.optimizeMesh();
    cylinder_mesh.calcRadius();

    vector_mesh.loadFromObjFile(
        getConfig()["AssetDirectoryFilepath"].get<std::string>()
            + getConfig()["VectorMeshFilepath"].get<std::string>());
    vector_mesh.optimizeMesh();
    vector_mesh.calcRadius(); //BUGFIX!!

    bond_mesh.loadFromObjFile(
        getConfig()["AssetDirectoryFilepath"].get<std::string>() + getConfig()["BondMeshFilepath"].get<std::string>());
    bond_mesh.calcRadius();
    bond_mesh.optimizeMesh();


    // destroy old meshes
    if (meshes.accumulated_mesh_) {
        resource_manager_->destroyBuffer(meshes.accumulated_mesh_->vertexBuffer_.handle_);
        resource_manager_->destroyBuffer(meshes.accumulated_mesh_->indexBuffer_.handle_);
        resource_manager_->buffers_.erase(meshes.accumulated_mesh_->vertexBuffer_.handle_);
        resource_manager_->buffers_.erase(meshes.accumulated_mesh_->indexBuffer_.handle_);
        meshes.accumulated_mesh_.reset(nullptr);
    }

    meshes = MeshMerger();
    meshes.accumulated_mesh_ = std::make_unique<Mesh>();

    // load meshes
    meshes.addMesh(atom_mesh, meshID::eAtom, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
        .addMesh(unit_cell_mesh, meshID::eUnitCell, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
        .addMesh(vector_mesh, meshID::eVector, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
        .addMesh(cylinder_mesh, meshID::eCylinder, atom_pipeline_->pipeline(), graphics_pipeline_layout_)
        .addMesh(bond_mesh, meshID::eBond, bond_pipeline_->pipeline(), graphics_pipeline_layout_);
    std::tie(meshes.accumulated_mesh_->vertexBuffer_, meshes.accumulated_mesh_->indexBuffer_)
        = resource_manager_->uploadMesh(*(meshes.accumulated_mesh_), upload_context_, graphics_queue_);

    scene_->setMeshes(&meshes);
}

FrameData &Engine::getCurrentFrame() {
    return frame_data_[framerate_control_.frame_number_%Swapchain::MAX_FRAMES_IN_FLIGHT];
}

const FrameData &Engine::getCurrentFrame() const {
    return frame_data_[framerate_control_.frame_number_%Swapchain::MAX_FRAMES_IN_FLIGHT];
}

uint32_t Engine::getCurrentFrameIndex() const {
    return framerate_control_.frame_number_%Swapchain::MAX_FRAMES_IN_FLIGHT;
}

void Engine::initDescriptors() {
    using buf = vk::BufferUsageFlagBits;

    descriptor_allocator_.init(logical_device_);
    layout_cache_.init(logical_device_);

    auto clear_draw_command_handle = resource_manager_->createBuffer(sizeof(GPUDrawCalls),
                                                                     buf::eTransferSrc | buf::eTransferDst,
                                                                     VMA_MEMORY_USAGE_GPU_ONLY);
    clear_draw_call_buffer_ = resource_manager_->createBufferResource(
        clear_draw_command_handle, 0, sizeof(GPUDrawCalls), {});

    auto indirect_dispatch_buffer_handle = resource_manager_->createBuffer(sizeof(vk::DispatchIndirectCommand),
                                                                           buf::eIndirectBuffer,
                                                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
    indirect_dispatch_buffer_ = resource_manager_->createBufferResource(
        indirect_dispatch_buffer_handle, 0, sizeof(vk::DispatchIndirectCommand), {});
    resource_manager_->mapBuffer(indirect_dispatch_buffer_handle);

    const size_t sceneDataBufferSize = Swapchain::MAX_FRAMES_IN_FLIGHT*paddedUniformBufferSize(sizeof(GPUSceneData));
    uint32_t scene_data_buffer_handle = resource_manager_->createBuffer(sceneDataBufferSize,
                                                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
    resource_manager_->mapBuffer(scene_data_buffer_handle);
    scene_data_buffer_ = resource_manager_->createBufferResource(
        scene_data_buffer_handle, 0, sizeof(GPUSceneData), vk::DescriptorType::eUniformBufferDynamic);

    for (auto &frame : frame_data_) {

        auto mouse_bucket_buffer_handle = resource_manager_->
            createBuffer(sizeof(mouse_buckets), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.mouse_bucket_buffer = resource_manager_->
            createBufferResource(mouse_bucket_buffer_handle,
                                 0,
                                 sizeof(mouse_buckets),
                                 vk::DescriptorType::eStorageBuffer);
        resource_manager_->mapBuffer(mouse_bucket_buffer_handle);

        auto cam_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUCamData), buf::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.cam_buffer = resource_manager_->
            createBufferResource(cam_buffer_handle, 0, sizeof(GPUCamData), vk::DescriptorType::eUniformBuffer);
        resource_manager_->mapBuffer(cam_buffer_handle);

        auto object_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUObjectData)*MAX_UNIQUE_OBJECTS, buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.object_buffer = resource_manager_->
            createBufferResource(object_buffer_handle,
                                 0,
                                 sizeof(GPUObjectData)*MAX_UNIQUE_OBJECTS,
                                 vk::DescriptorType::eStorageBuffer);
        resource_manager_->mapBuffer(object_buffer_handle);

        auto cull_data_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUCullData), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.cull_data_buffer = resource_manager_->
            createBufferResource(cull_data_buffer_handle, 0, sizeof(GPUCullData), vk::DescriptorType::eStorageBuffer);
        resource_manager_->mapBuffer(cull_data_buffer_handle);

        auto offset_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUOffsets), buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.offset_buffer = resource_manager_->
            createBufferResource(offset_buffer_handle, 0, sizeof(GPUOffsets), vk::DescriptorType::eStorageBuffer);
        resource_manager_->mapBuffer(offset_buffer_handle);

        auto instance_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUInstance)*MAX_UNIQUE_OBJECTS, buf::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.instance_buffer = resource_manager_->
            createBufferResource(instance_buffer_handle,
                                 0,
                                 sizeof(GPUInstance)*MAX_UNIQUE_OBJECTS,
                                 vk::DescriptorType::eStorageBuffer);
        resource_manager_->mapBuffer(instance_buffer_handle);

        auto final_instance_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUFinalInstance)*MAX_UNIQUE_OBJECTS*27,
                         buf::eStorageBuffer,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.final_instance_buffer = resource_manager_->
            createBufferResource(final_instance_buffer_handle,
                                 0,
                                 sizeof(GPUFinalInstance)*MAX_UNIQUE_OBJECTS*27,
                                 vk::DescriptorType::eStorageBuffer);

        auto draw_call_buffer_handle = resource_manager_->
            createBuffer(sizeof(GPUDrawCalls),
                         buf::eStorageBuffer | buf::eIndirectBuffer | buf::eTransferDst,
                         VMA_MEMORY_USAGE_GPU_ONLY);
        frame.draw_call_buffer = resource_manager_->
            createBufferResource(draw_call_buffer_handle, 0, sizeof(GPUDrawCalls), vk::DescriptorType::eStorageBuffer);

        // 3. Bondage
        DescriptorBuilder::begin(&layout_cache_, &descriptor_allocator_)
            .bindBuffer(0,
                        frame.cam_buffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .bindBuffer(1,
                        scene_data_buffer_,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .bindBuffer(2,
                        frame.object_buffer,
                        vk::ShaderStageFlagBits::eVertex)
            .bindBuffer(3,
                        frame.mouse_bucket_buffer,
                        vk::ShaderStageFlagBits::eFragment)
            .bindBuffer(4,
                        frame.final_instance_buffer,
                        vk::ShaderStageFlagBits::eVertex)
            .bindBuffer(5,
                        frame.offset_buffer,
                        vk::ShaderStageFlagBits::eVertex)
            .build(frame.global_descriptor_set, graphics_descriptor_set_layout);

        vk::DescriptorImageInfo position_image_descriptor{
            swapchain_->g_buffer_sampler, swapchain_->offscreen_framebuffer.position.view, vk::ImageLayout::eShaderReadOnlyOptimal};
        vk::DescriptorImageInfo normal_image_descriptor{
            swapchain_->g_buffer_sampler, swapchain_->offscreen_framebuffer.normal.view, vk::ImageLayout::eShaderReadOnlyOptimal};
        vk::DescriptorImageInfo albedo_image_descriptor{
            swapchain_->g_buffer_sampler, swapchain_->offscreen_framebuffer.albedo.view, vk::ImageLayout::eShaderReadOnlyOptimal};

        DescriptorBuilder::begin(&layout_cache_, &descriptor_allocator_)
            .bindImage(0, &position_image_descriptor, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
            .bindImage(1, &normal_image_descriptor, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
            .bindImage(2, &albedo_image_descriptor, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
            .build(frame.deferred_descriptor_set, deferred_descriptor_set_layout);

        DescriptorBuilder::begin(&layout_cache_, &descriptor_allocator_)
            .bindBuffer(0,
                        frame.object_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .bindBuffer(1,
                        frame.cull_data_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .bindBuffer(2,
                        frame.instance_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .bindBuffer(3,
                        frame.final_instance_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .bindBuffer(4,
                        frame.draw_call_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .bindBuffer(5,
                        frame.offset_buffer,
                        vk::ShaderStageFlagBits::eCompute)
            .build(frame.compute_shader_set, culling_descriptor_set_layout);

        resource_manager_->clearBuffer(frame.mouse_bucket_buffer);
    }
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

void Engine::beginPresentRenderPass(vk::CommandBuffer &cmd, uint32_t swapchain_index) {

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
        viewport{0.f, 0.f, static_cast<float>(swapchain_->width()), static_cast<float>(swapchain_->height()), 0.f, 1.f};
    vk::Rect2D scissor{{0, 0}, swapchain_->extent()};
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);
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

void Engine::loadExperiment(int experiment_id) {
    assert(scene_->visManager!=nullptr
               && "vis manager must be initialized, i.e. a database must be connected before loading an experiment");

    if (database_state==eOld && experiment_id==scene_->visManager->getActiveExperiment()) {
        std::cout << "already loaded experiment with id: " << experiment_id << std::endl;
        return;
    }

    scene_->visManager->load(experiment_id);
    float dist = glm::length(scene_->visManager->data().unitCellGLM*glm::vec3(1.f, 1.f, 1.f));
    camera_->alignPerspectivePositionToSystemCenter(dist*1.5f);
    experiment_state_ = eNew;
}

void Engine::unloadExperiment() {
    assert(scene_->visManager!=nullptr
               && "vis manager must be initialized, i.e. a database must be connected before unloading an experiment");
    scene_->visManager->unload();
    experiment_state_ = eNone;
}

void Engine::connectToDB() {
    assert(scene_->visManager==nullptr
               && "vis manager must be uninitialized, i.e. a database must be disconnected before connecting to a new one");
    scene_->visManager = std::make_unique<VisDataManager>(db_filepath_);
    ui->experimentsNeedRefresh = true;
    database_state = eNew;
    if (scene_->visManager->getExperimentCount()==1) {
        loadExperiment(scene_->visManager->getFirstExperimentID());
    }
}

void Engine::disconnectFromDB() {

    // just kill the vis manager
    scene_->visManager.reset(nullptr);
    // reset ui per db state
    ui->loadedSettings.clear();
    ui->loadedEventsText.clear();
    ui->experiments_.experimentSystemSettingIDs.clear();

    database_state = eNone;
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

        if (ui->preferencesWindowVisible) {
            ui->preferencesWindowVisible = false;
        } else if (experiment_state_!=eNone) {
            if (ui_mode_==uiMode::eSelectAndTag) { cleanupSelectAndTagMode(); }
            if (ui_mode_==uiMode::eMeasure) { cleanupMeasurementMode(); }
        }
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

    resource_manager_->writeToBuffer(getCurrentFrame().cam_buffer, &ubo, sizeof(GPUCamData));
}

void Engine::writeSceneBuffer() {
    //write mouse coords to push constant
    double mouse_coords[2] = {0, 0}; // for float to double conversion
    glfwGetCursorPos(window_->glfwWindow_, &mouse_coords[0], &mouse_coords[1]);
    scene_data_.mouse_coords[0] = static_cast<float>(mouse_coords[0]);
    scene_data_.mouse_coords[1] = static_cast<float>(mouse_coords[1]);
    scene_data_.point_lights[0].position = glm::vec4(camera_->GetPosition(), 1.f);
    scene_data_.point_lights[0].lightColor = glm::vec4(1.f, 1.f, 1.f, 50.f);
    scene_data_.ambientColor = glm::vec4(1.f, 1.f, 1.f, 0.02f);

    scene_data_buffer_.descriptor_buffer_info_.offset =
        paddedUniformBufferSize(sizeof(GPUSceneData))*getCurrentFrameIndex();
    resource_manager_->writeToBuffer(scene_data_buffer_, &scene_data_, sizeof(GPUSceneData));
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

    resource_manager_->writeToBuffer(getCurrentFrame().cull_data_buffer, &cullData, sizeof(GPUCullData));
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
    resource_manager_->stageBuffer(&draws,
                                   sizeof(GPUDrawCalls),
                                   clear_draw_call_buffer_,
                                   upload_context_,
                                   graphics_queue_);
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
    resource_manager_->writeToBuffer(getCurrentFrame().offset_buffer, &gpu_offsets, sizeof(GPUOffsets));
}

void Engine::writeObjectAndInstanceBuffer() {
    auto *objectSSBO = (GPUObjectData *) resource_manager_->getMappedData(getCurrentFrame().object_buffer.handle_);
    auto *instanceSSBO = (GPUInstance *) resource_manager_->getMappedData(getCurrentFrame().instance_buffer.handle_);
    scene_->writeObjectAndInstanceBuffer(objectSSBO, instanceSSBO, GetMovieFrameIndex(), selected_object_index_);
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
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eCompute,
        compute_shader_module,
        "main",
        nullptr);

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

void Engine::resetDrawData(vk::CommandBuffer &cmd,
                           BufferResource src,
                           BufferResource dst,
                           vk::DeviceSize size) const {

    auto &src_buffer = resource_manager_->getBuffer(src);
    auto &dst_buffer = resource_manager_->getBuffer(dst);

    vk::BufferCopy copy{src.descriptor_buffer_info_.offset, dst.descriptor_buffer_info_.offset, size};
    cmd.copyBuffer(src_buffer.buffer_, dst_buffer.buffer_, copy);

    using acs = vk::AccessFlagBits;
    vk::BufferMemoryBarrier barrier
        {acs::eTransferWrite, acs::eShaderRead | acs::eShaderWrite, graphics_queue_family_, graphics_queue_family_,
         dst_buffer.buffer_, dst.descriptor_buffer_info_.offset, size};
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
    resource_manager_->writeToBuffer(indirect_dispatch_buffer_, &command, sizeof(vk::DispatchIndirectCommand));
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
    glm::vec4 &col = scene_->gConfig.catalyst_color_;
    getConfig()["CatalystColor"] = std::array<float, 4>{col.r, col.g, col.b, col.a};
    col = scene_->gConfig.chemical_color_;
    getConfig()["ChemicalColor"] = std::array<float, 4>{col.r, col.g, col.b, col.a};
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
    getConfig()["ShowFPS"] = ui->fpsVisible;

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

        float element_radius =
            scene_->visManager->data().elementInfos.find(scene_->visManager->data().tags(i) & 255)->second.atomRadius;
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

void Engine::beginOffscreenRenderPass(vk::CommandBuffer &cmd) {
    std::array<vk::ClearValue, 4> clearValues{
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
        vk::ClearDepthStencilValue(1.0f, 0)
    };

    vk::RenderPassBeginInfo renderPassBeginInfo{
        swapchain_->offscreen_framebuffer.renderPass,
        swapchain_->offscreen_framebuffer.framebuffer,
        vk::Rect2D{vk::Offset2D(0, 0),
                   vk::Extent2D(swapchain_->width(), swapchain_->height())},
        clearValues};

    cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    vk::Viewport
        viewport{0.f, 0.f, static_cast<float>(swapchain_->width()), static_cast<float>(swapchain_->height()), 0.f, 1.f};
    vk::Rect2D scissor{{0, 0}, swapchain_->extent()};
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);
}

} //namespace rcc