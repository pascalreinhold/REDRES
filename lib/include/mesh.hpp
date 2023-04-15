//
// Created by x on 11/19/22.
//

#pragma once

#include "buffer.hpp"
#include "vulkan_types.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/glm.hpp>
#include <vector>

#include <memory>
#include <map>

namespace rcc {

enum meshID {
    eAtom = 0,
    eUnitCell = 1,
    eVector = 2,
    eCylinder = 3,
    eBond = 4
};

struct VertexDescription {
  std::vector<vk::VertexInputBindingDescription> bindings_;
  std::vector<vk::VertexInputAttributeDescription> attributes_;
  vk::PipelineVertexInputStateCreateFlags flags_ = vk::PipelineVertexInputStateCreateFlags();
};

struct BasicVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  static VertexDescription getDescription();
};

struct TexturedVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static VertexDescription getDescription();
};

struct MeshInterface {
  BufferResource vertexBuffer_;
  BufferResource indexBuffer_;
  virtual uint32_t getVertexCount() = 0;
  virtual uint32_t getIndicesCount() = 0;
};

//template<typename VertexType>
struct Mesh : public MeshInterface {
  std::vector<BasicVertex> vertices_;
  std::vector<uint32_t> indices_;

  float radius;

  uint32_t getVertexCount() override { return vertices_.size(); };
  uint32_t getIndicesCount() override { return indices_.size(); };

  void optimizeMesh();

  void calcRadius();

  void loadFromObjFile(const std::string &filepath);
  void createUnitCellMesh(const glm::mat3 &b);
  //void createFrustumMesh();
  void createBeam(const glm::vec3 p1, glm::vec3 p2, glm::vec3 right_dir, glm::vec3 up_dir, float thickness);
};

class MeshMerger {
 public:
  struct MeshInfo {
    vk::Pipeline pipeline;
    vk::PipelineLayout pipeline_layout;
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t firstVertex;
    float radius;
  };

  std::map<meshID, MeshInfo> meshInfos;
  MeshMerger &addMesh(const Mesh &mesh, meshID mesh_id, vk::Pipeline pipeline, vk::PipelineLayout layout);

  std::unique_ptr<Mesh> accumulated_mesh_;
};

struct TexturedMesh : public MeshInterface {
  std::vector<TexturedVertex> vertices_;
  std::vector<uint32_t> indices_;

  uint32_t getVertexCount() override { return vertices_.size(); };
  uint32_t getIndicesCount() override { return indices_.size(); };

  void loadFromObjFile(const std::string &filepath);
};

// Will be used to automate drawing of different materials
struct MaterialBatch {
  vk::Pipeline pipeline_;
  vk::PipelineLayout pipeline_layout_;
  uint32_t first_element_, element_count_;
};

struct MeshPushConstants {
  glm::mat4 model_matrix;
  glm::vec4 parameters;
};

}

