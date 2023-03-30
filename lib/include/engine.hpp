#pragma once

//my files
#include "scene.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "descriptors.hpp"
#include "window.hpp"
#include "pipeline.hpp"
#include "vulkan_types.hpp"
#include "swapchain.hpp"
#include "visualization_data_loader.hpp"

//lib
#include "json.hpp"

//stdlib
#include <stack>
#include <functional>
#include <chrono>

namespace rcc {

constexpr uint32_t FRAMES_IN_FLIGHT = 3;
constexpr int MAX_UNIQUE_OBJECTS = 15000;

struct DeletionStack {
  std::stack<std::function<void()>> delete_calls;
  void push(std::function<void()> &&function) {
    delete_calls.push(function);
  }
  void flush() {
    while (!delete_calls.empty()) {
      delete_calls.top()();
      delete_calls.pop();
    }
  }
};

class Engine {
 public:
  Engine(const char *db_filepath, const char *asset_dir_filepath);
  ~Engine();

  void init();
  void render();
  void cleanup();
  void run();

  void mouseButtonCallback(int button, int action, int mods);
  void keyCallback(int key, int scancode, int action, int mods);
  void scrollCallback(double y_offset);

  static nlohmann::json &getConfig();
  static Engine *keyboardBackedEngine;

 private:
  std::unique_ptr<class UserInterface> ui;

  // buffers
  FrameData frame_data_[FRAMES_IN_FLIGHT];
  AllocatedBuffer indirect_dispatch_buffer_{};
  AllocatedBuffer scene_data_buffer_{};
  AllocatedBuffer clear_draw_call_buffer_{};

  // descriptors
  DescriptorLayoutCache layout_cache_;
  DescriptorAllocator descriptor_allocator_;

  // graphics pipelines and descriptor sets
  vk::PipelineLayout graphics_pipeline_layout_;
  std::unique_ptr<Pipeline> atom_pipeline_, bond_pipeline_, atom_pipeline_iso_, bond_pipeline_iso_;
  vk::DescriptorSetLayout graphics_descriptor_set_layout;
  SpecializationConstants specialization_constants_{};

  // compute pipeline and descriptor set
  vk::Pipeline culling_compute_pipeline_;
  vk::PipelineLayout culling_compute_pipeline_layout_;
  vk::DescriptorSetLayout culling_descriptor_set_layout;
  vk::Pipeline createComputePipeline(const std::string &shader_path, const vk::PipelineLayout &compute_pipeline_layout);

  // transfers
  UploadContext upload_context_;
  void immediateSubmit(std::function<void(vk::CommandBuffer cmd)> &&function);

  // rendering
  std::unique_ptr<Window> window_;
  std::unique_ptr<Scene> scene_;

  // meshes
  MeshMerger meshes;
  void uploadMesh(Mesh &mesh, AllocatedBuffer &indexBuffer, AllocatedBuffer &vertexBuffer);
  void loadMeshes();

  // rendering
  void draw(vk::CommandBuffer &cmd) const;
  void beginRenderPass(vk::CommandBuffer &cmd, uint32_t swapchain_index);
  void runCullComputeShader(vk::CommandBuffer cmd);

  // scene
  std::array<float, 4> clearColor{0.f, 0.f, 0.f, 0.f};
  GPUSceneData scene_data_;

  // viewing state control
  void toggleFrameControlMode() { framerate_control_.manualFrameControl = !framerate_control_.manualFrameControl; }
  void toggleCameraMode();

  //Getters
  int GetMovieFrameIndex() const { return static_cast<int>(framerate_control_.movie_frame_index_); }
  FrameData &getCurrentFrame();
  const FrameData &getCurrentFrame() const;
  uint32_t getCurrentFrameIndex() const;

  // mouse handling
  uint32_t mouse_buckets[RCC_MOUSE_BUCKET_COUNT] = {};
  void processMousePickingBuffer();
  void processMouseDrag();
  void selectAtomsWithRect(glm::vec2 start, glm::vec2 end, int frame_index);
  bool bReadMousePickingBuffer_ = false;
  int selected_object_index_ = -1;

  // misc
  int max_cell_count_;
  bool isCullingEnabled = true;

  // camera
  std::unique_ptr<Camera> camera_;
  bool bDragRotateCam_ = false;

  void enterEventMode(int eventID);
  void leaveEventMode();
  void GetOptimalCameraPerspective();
  glm::vec3 getCenterCoords();

  // frame control
  struct {
    bool isSimulationLooped = true;
    bool manualFrameControl = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> currentTime;
    float frame_time_{0.016};
    Averager<float> avgFrameTime;
    int max_framerate_ = 200;
    int movie_framerate_ = 200;
    int frame_number_{0};
    float movie_frame_index_{0};
  } framerate_control_;

  // file management
  std::string db_filepath_;
  std::string asset_dir_filepath_ = "/usr/share/gpu_driven_rcc/";
  std::string settings_filepath_ = "/assets/settings.json";
  std::string default_settings_filepath_ = "/assets/default_settings.json";
  void DumpSettingsJson(const std::string &filepath);

  // vulkan objects
  vk::Instance instance_;
  VkDebugUtilsMessengerEXT debug_messenger_{};
  vk::SurfaceKHR surface_;
  vk::PhysicalDevice physical_device_{};
  vk::PhysicalDeviceProperties gpu_properties_;
  vk::Device logical_device_;
  VmaAllocator allocator_{};
  uint32_t graphics_queue_family_{};
  vk::Queue graphics_queue_;

  // rcc vulkan wrapper
  std::unique_ptr<Swapchain> swapchain_;

  DeletionStack main_destruction_stack_;

  // init functions
  void initVulkan();
  void recreateSwapchain();
  void initCommands();
  void initSyncStructures();
  void initDescriptors();
  void initPipelines();
  void initComputePipelines();
  void initScene();
  void initCamera();
  void initVisData();
  void initGlfwCallbacks();

  // buffer manipulation
  AllocatedBuffer createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage) const;
  size_t paddedUniformBufferSize(size_t old_size) const;
  void stageBuffer(void *src, AllocatedBuffer dest, vk::DeviceSize size, vk::DeviceSize offset);
  void writeClearDrawCallBuffer();
  void writeIndirectDispatchBuffer();
  void writeCameraBuffer();
  void writeSceneBuffer();
  void writeObjectAndInstanceBuffer();
  void writeCullBuffer();
  void writeOffsetBuffer();
  void writeToBuffer(AllocatedBuffer &buffer, uint32_t range, void *data, uint32_t offset = 0) const;
  void readFromBuffer(AllocatedBuffer &buffer, uint32_t range, void *data, uint32_t offset = 0) const;
  void readFromBufferAndClearIt(AllocatedBuffer &buffer, uint32_t range, void *data, uint32_t offset = 0) const;
  void resetDrawData(vk::CommandBuffer &cmd, AllocatedBuffer src, AllocatedBuffer dst,
                     vk::DeviceSize size, vk::DeviceSize src_offset = 0, vk::DeviceSize dst_offset = 0) const;

  friend class UserInterface;
  GPUOffsets getOffsets();
};

}