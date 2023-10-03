#pragma once

//my files
#include "utils.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "descriptors.hpp"
#include "vulkan_types.hpp"
#include "buffer.hpp"

//lib
#include "json.hpp"

//stdlib
#include <stack>
#include <functional>
#include <chrono>

namespace rcc {

// forward declarations
class Scene;

enum State {
  eNone,
  eNew,
  eOld
};

enum uiMode {
  eSelectAndTag,
  eMeasure
};

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

  static Engine *keyboardBackedEngine;
  void mouseButtonCallback(int button, int action, int mods);
  void keyCallback(int key, int scancode, int action, int mods);
  void scrollCallback(double y_offset);

  static nlohmann::json &getConfig();

 private:
  // ui
  std::unique_ptr<class UserInterface> ui;
  std::unique_ptr<class ResourceManager> resource_manager_;
  uiMode ui_mode_ = eMeasure;

  // buffers
  FrameData frame_data_[FRAMES_IN_FLIGHT];
  BufferResource scene_data_buffer_;
  BufferResource indirect_dispatch_buffer_{};
  BufferResource clear_draw_call_buffer_{};

  // descriptors
  DescriptorLayoutCache layout_cache_;
  DescriptorAllocator descriptor_allocator_;

  // graphics pipelines and descriptor sets
  vk::PipelineLayout graphics_pipeline_layout_;
  std::unique_ptr<class Pipeline> atom_pipeline_, bond_pipeline_, atom_pipeline_iso_, bond_pipeline_iso_;
  vk::DescriptorSetLayout graphics_descriptor_set_layout;
  SpecializationConstants specialization_constants_{};

  // compute pipeline and descriptor set
  vk::Pipeline culling_compute_pipeline_;
  vk::PipelineLayout culling_compute_pipeline_layout_;
  vk::DescriptorSetLayout culling_descriptor_set_layout;
  vk::Pipeline createComputePipeline(const std::string &shader_path, const vk::PipelineLayout &compute_pipeline_layout);

  // transfers
  UploadContext upload_context_;

  // rendering
  std::unique_ptr<class Window> window_;
  std::unique_ptr<Scene> scene_;

  // meshes
  MeshMerger meshes;
  void uploadMesh(Mesh &mesh, BufferResource &indexBuffer, BufferResource &vertexBuffer);
  void loadMeshes();

  // rendering
  void draw(vk::CommandBuffer &cmd);
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
  void cleanupMeasurementMode();
  void cleanupSelectAndTagMode();
  void processMouseDrag();
  void selectAtomsWithRect(glm::vec2 start, glm::vec2 end, int frame_index);
  bool bReadMousePickingBuffer_ = false;
  int selected_object_index_ = -1;
  std::deque<int> selected_atom_numbers_;

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

  // control flow vars
  State experiment_state_ = eNone;
  State database_state = eNone;

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
  std::unique_ptr<class Swapchain> swapchain_;
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
  void initGlfwCallbacks();

    // buffer manipulation
    BufferResource createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage) const;
    size_t paddedUniformBufferSize(size_t old_size) const;
    void writeClearDrawCallBuffer();
    void writeIndirectDispatchBuffer();
    void writeCameraBuffer();
    void writeSceneBuffer();
    void writeObjectAndInstanceBuffer();
    void writeCullBuffer();
    void writeOffsetBuffer();
    void resetDrawData(vk::CommandBuffer &cmd,
                     BufferResource src,
                     BufferResource dst,
                     vk::DeviceSize size) const;

    GPUOffsets getOffsets();
    void loadExperiment(int experiment_id);
    void unloadExperiment();
    void connectToDB();
    void disconnectFromDB();

    friend class UserInterface;
};

}