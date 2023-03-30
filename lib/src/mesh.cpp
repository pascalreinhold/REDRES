#include "mesh.hpp"
#include "engine.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "meshoptimizer.h"

#include <string>
#include <iostream>

namespace rcc {
VertexDescription BasicVertex::getDescription() {
  VertexDescription description;

  //we will have just 1 vertex buffer binding, with a per-vertex rate
  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(BasicVertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings_.emplace_back(mainBinding);

  //Position will be stored at Location 0
  VkVertexInputAttributeDescription position_attribute = {};
  position_attribute.binding = 0;
  position_attribute.location = 0;
  position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  position_attribute.offset = offsetof(BasicVertex, position);

  //Normal will be stored at Location 1
  VkVertexInputAttributeDescription normal_attribute = {};
  normal_attribute.binding = 0;
  normal_attribute.location = 1;
  normal_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  normal_attribute.offset = offsetof(BasicVertex, normal);

  //Color will be stored at Location 2
  VkVertexInputAttributeDescription color_attribute = {};
  color_attribute.binding = 0;
  color_attribute.location = 2;
  color_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  color_attribute.offset = offsetof(BasicVertex, color);

  description.attributes_.emplace_back(position_attribute);
  description.attributes_.emplace_back(normal_attribute);
  description.attributes_.emplace_back(color_attribute);

  return description;
}

void Mesh::loadFromObjFile(const std::string &filepath) {

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), nullptr);

  //if (!warn.empty()) std::cout << "TinyObjLoader Warning: " << warn << "\n";
  if (!err.empty()) std::cerr << "TinyObjLoader Error: " << err << "\n";

  std::vector<BasicVertex> unindexedVertices;

  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      int fv = 3;
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

        //vertex position
        tinyobj::real_t vx = attrib.vertices[3*idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3*idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3*idx.vertex_index + 2];
        //vertex normal
        tinyobj::real_t nx = attrib.normals[3*idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3*idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3*idx.normal_index + 2];
        //vertex color
        tinyobj::real_t cx = attrib.colors[3*idx.vertex_index + 0];
        tinyobj::real_t cy = attrib.colors[3*idx.vertex_index + 1];
        tinyobj::real_t cz = attrib.colors[3*idx.vertex_index + 2];

        //copy it into our vertex
        BasicVertex new_vert{};
        new_vert.position.x = vx;
        new_vert.position.y = vy;
        new_vert.position.z = vz;

        new_vert.normal.x = nx;
        new_vert.normal.y = ny;
        new_vert.normal.z = nz;

        new_vert.color.x = cx;
        new_vert.color.y = cy;
        new_vert.color.z = cz;

        new_vert.color = glm::normalize(new_vert.color);
        unindexedVertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }

  // create indices
  uint32_t index_count = unindexedVertices.size();
  std::vector<uint32_t> remap(index_count);
  size_t vertex_count = meshopt_generateVertexRemap(&remap[0],
                                                    nullptr,
                                                    index_count,
                                                    unindexedVertices.data(),
                                                    index_count,
                                                    sizeof(BasicVertex));

  indices_.resize(index_count);
  meshopt_remapIndexBuffer(indices_.data(), nullptr, index_count, remap.data());

  vertices_.resize(vertex_count);
  meshopt_remapVertexBuffer(vertices_.data(), unindexedVertices.data(), index_count, sizeof(BasicVertex), remap.data());

}

void Mesh::createBeam(const glm::vec3 p1, glm::vec3 p2, glm::vec3 right_dir, glm::vec3 up_dir, float thickness) {

  auto dir = normalize(p2 - p1);
  auto normal1 = glm::normalize(right_dir);
  auto normal2 = glm::normalize(up_dir);

  BasicVertex v1 = {}, v2 = {}, v3 = {}, v4 = {}, v5 = {}, v6 = {};

  //set of faces 1
  v1.position = p1 - thickness*dir - thickness*normal1 - thickness*normal2;
  v2.position = p1 - thickness*dir - thickness*normal1 + thickness*normal2;
  v3.position = p2 + thickness*dir - thickness*normal1 - thickness*normal2;
  v4.position = p2 + thickness*dir - thickness*normal1 + thickness*normal2;
  v5.position = p1 - thickness*dir - thickness*normal1 + thickness*normal2;
  v6.position = p2 + thickness*dir - thickness*normal1 - thickness*normal2;
  v1.normal = v2.normal = v3.normal = v4.normal = v5.normal = v6.normal = -normal1;
  vertices_.insert(vertices_.end(), {v1, v2, v3, v4, v5, v6});

  v1.position = p1 - thickness*dir + thickness*normal1 - thickness*normal2;
  v2.position = p1 - thickness*dir + thickness*normal1 + thickness*normal2;
  v3.position = p2 + thickness*dir + thickness*normal1 - thickness*normal2;
  v4.position = p2 + thickness*dir + thickness*normal1 + thickness*normal2;
  v5.position = p1 - thickness*dir + thickness*normal1 + thickness*normal2;
  v6.position = p2 + thickness*dir + thickness*normal1 - thickness*normal2;
  v1.normal = v2.normal = v3.normal = v4.normal = v5.normal = v6.normal = normal1;
  vertices_.insert(vertices_.end(), {v1, v2, v3, v4, v5, v6});

  // set of parallel faces 2
  v1.position = p1 - thickness*dir - thickness*normal2 - thickness*normal1;
  v2.position = p1 - thickness*dir - thickness*normal2 + thickness*normal1;
  v3.position = p2 + thickness*dir - thickness*normal2 - thickness*normal1;
  v4.position = p2 + thickness*dir - thickness*normal2 + thickness*normal1;
  v5.position = p1 - thickness*dir - thickness*normal2 + thickness*normal1;
  v6.position = p2 + thickness*dir - thickness*normal2 - thickness*normal1;
  v1.normal = v2.normal = v3.normal = v4.normal = v5.normal = v6.normal = -normal2;
  vertices_.insert(vertices_.end(), {v1, v2, v3, v4, v5, v6});

  v1.position = p1 - thickness*dir + thickness*normal2 - thickness*normal1;
  v2.position = p1 - thickness*dir + thickness*normal2 + thickness*normal1;
  v3.position = p2 + thickness*dir + thickness*normal2 - thickness*normal1;
  v4.position = p2 + thickness*dir + thickness*normal2 + thickness*normal1;
  v5.position = p1 - thickness*dir + thickness*normal2 + thickness*normal1;
  v6.position = p2 + thickness*dir + thickness*normal2 - thickness*normal1;
  v1.normal = v2.normal = v3.normal = v4.normal = v5.normal = v6.normal = normal2;
  vertices_.insert(vertices_.end(), {v1, v2, v3, v4, v5, v6});

  for (auto &vertex : vertices_) vertex.color = glm::vec3(1.f);
}

void Mesh::createUnitCellMesh(const glm::mat3 &b) {

  auto &b1 = b[0];
  auto &b2 = b[1];
  auto &b3 = b[2];

  float thickness = Engine::getConfig()["UnitCellThickness"].get<float>()
      *std::min({glm::length(b1), glm::length(b2), glm::length(b3)});
  createBeam(glm::vec3(0.f), b1, b3, b2, thickness);
  createBeam(b3, b3 + b1, b3, b2, thickness);
  createBeam(b2, b1 + b2, b3, b2, thickness);
  createBeam(b3 + b2, b3 + b1 + b2, b3, b2, thickness);

  createBeam(glm::vec3(0.f), b2, -b3, b1, thickness);
  createBeam(b3, b3 + b2, -b3, b1, thickness);
  createBeam(b1, b1 + b2, -b3, b1, thickness);
  createBeam(b3 + b1, b3 + b1 + b2, b1, -b3, thickness);

  createBeam(glm::vec3(0.f), b3, -b1, b2, thickness);
  createBeam(b1, b3 + b1, -b1, b2, thickness);
  createBeam(b2, b3 + b2, -b1, b2, thickness);
  createBeam(b1 + b2, b3 + b1 + b2, -b1, b2, thickness);

  indices_.resize(vertices_.size());
  for (int i = 0; i < indices_.size(); i++) {
    indices_[i] = i;
  }
  /*
  // create indices
  std::vector<BasicVertex> unindexedVertices = vertices_;
  vertices_.clear();

  uint32_t index_count = unindexedVertices.size();
  std::vector<uint32_t> remap(index_count);
  size_t vertex_count = meshopt_generateVertexRemap(&remap[0], nullptr, index_count, unindexedVertices.data(), index_count, sizeof(TexturedVertex));

  indices_.resize(index_count);
  meshopt_remapIndexBuffer(indices_.data(), nullptr, index_count, remap.data());

  vertices_.resize(vertex_count);
  meshopt_remapVertexBuffer(vertices_.data(), unindexedVertices.data(), index_count, sizeof(TexturedVertex), remap.data());
  */

}

/*
void Mesh::createFrustumMesh() {

  float width = Engine::getConfig()["WindowWidth"].get<float>();
  float height = Engine::getConfig()["WindowHeight"].get<float>();
  float near = Engine::getConfig()["NearPlane"].get<float>();
  float far = Engine::getConfig()["FarPlane"].get<float>();
  float fovy = glm::radians(Engine::getConfig()["FOVY"].get<float>());
  float aspectRatio = width / height;

  float top = sqrt(near * near * (1 - cos(fovy)) / (1 + cos(fovy)));
  float bottom = -top;
  float right = top * aspectRatio;
  float left = -right;

  float thickness = 0.05;

  const auto origin = glm::vec3(0, 0, 0);
  const auto nearTopLeft = glm::vec3(left ,top, near);
  const auto nearBottomLeft = glm::vec3(left ,bottom, near);
  const auto nearTopRight = glm::vec3(right ,top, near);
  const auto nearBottomRight = glm::vec3(right ,bottom, near);

  const auto farTopLeft = nearTopLeft / near * far;
  const auto farBottomLeft = nearBottomLeft / near * far;
  const auto farTopRight = nearTopRight / near * far;
  const auto farBottomRight = nearBottomRight / near * far;

  const auto LeftSideRightDir = glm::cross(nearTopLeft, nearBottomLeft);
  const auto RightSideRightDir = glm::cross(nearTopRight, nearBottomRight);

  const auto TopSideUpDir = glm::cross(nearTopLeft, nearTopRight);
  const auto BottomSideUpDir = glm::cross(nearBottomLeft, nearBottomRight);

  //
  const auto defaultRight = glm::vec3(0,0,-1);
  const auto defaultUp = glm::vec3(0,-1,0);
  const auto weirdUp = glm::vec3(1,0,0);

  createBeam(nearTopLeft, nearTopRight, defaultRight, defaultUp, thickness);
  createBeam(nearBottomLeft, nearBottomRight, defaultRight, defaultUp, thickness);
  createBeam(nearTopLeft, nearBottomLeft, defaultRight, weirdUp, thickness);
  createBeam(nearTopRight, nearBottomRight, defaultRight, weirdUp, thickness);

  // origin to far plane connections
  createBeam(origin, farTopLeft, LeftSideRightDir, TopSideUpDir, thickness);
  createBeam(origin, farTopRight, RightSideRightDir, TopSideUpDir, thickness);
  createBeam(origin, farBottomLeft, LeftSideRightDir, BottomSideUpDir, thickness);
  createBeam(origin, farBottomRight, RightSideRightDir, BottomSideUpDir, thickness);
}
*/

VertexDescription TexturedVertex::getDescription() {
  VertexDescription description;

  //we will have just 1 vertex buffer binding, with a per-vertex rate
  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(TexturedVertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings_.emplace_back(mainBinding);

  //Position will be stored at Location 0
  VkVertexInputAttributeDescription position_attribute = {};
  position_attribute.binding = 0;
  position_attribute.location = 0;
  position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  position_attribute.offset = offsetof(TexturedVertex, position);

  //Normal will be stored at Location 1
  VkVertexInputAttributeDescription normal_attribute = {};
  normal_attribute.binding = 0;
  normal_attribute.location = 1;
  normal_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  normal_attribute.offset = offsetof(TexturedVertex, normal);

  //Color will be stored at Location 2
  VkVertexInputAttributeDescription color_attribute = {};
  color_attribute.binding = 0;
  color_attribute.location = 2;
  color_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  color_attribute.offset = offsetof(TexturedVertex, color);


  //Texture Coords will be stored at Location 3
  VkVertexInputAttributeDescription uv_attribute = {};
  uv_attribute.binding = 0;
  uv_attribute.location = 3;
  uv_attribute.format = VK_FORMAT_R32G32_SFLOAT;
  uv_attribute.offset = offsetof(TexturedVertex, uv);

  description.attributes_.emplace_back(position_attribute);
  description.attributes_.emplace_back(normal_attribute);
  description.attributes_.emplace_back(color_attribute);
  description.attributes_.emplace_back(uv_attribute);
  return description;
}

void TexturedMesh::loadFromObjFile(const std::string &filepath) {

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), nullptr);

  if (!warn.empty()) std::cout << "TinyObjLoader Warning: " << warn << "\n";
  if (!err.empty()) std::cerr << "TinyObjLoader Error: " << err << "\n";

  std::vector<TexturedVertex> unindexedVertices;

  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      int fv = 3;
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

        //vertex position
        tinyobj::real_t vx = attrib.vertices[3*idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3*idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3*idx.vertex_index + 2];
        //vertex normal
        tinyobj::real_t nx = attrib.normals[3*idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3*idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3*idx.normal_index + 2];
        //vertex color
        tinyobj::real_t cx = attrib.colors[3*idx.vertex_index + 0];
        tinyobj::real_t cy = attrib.colors[3*idx.vertex_index + 1];
        tinyobj::real_t cz = attrib.colors[3*idx.vertex_index + 2];

        //vertex uv
        tinyobj::real_t ux = attrib.texcoords[2*idx.texcoord_index + 0];
        tinyobj::real_t uy = attrib.texcoords[2*idx.texcoord_index + 1];

        //copy it into our vertex
        TexturedVertex new_vert{};
        new_vert.position.x = vx;
        new_vert.position.y = vy;
        new_vert.position.z = vz;

        new_vert.normal.x = nx;
        new_vert.normal.y = ny;
        new_vert.normal.z = nz;

        new_vert.color.x = cx;
        new_vert.color.y = cy;
        new_vert.color.z = cz;

        new_vert.uv.x = ux;
        new_vert.uv.y = 1 - uy;

        new_vert.color = glm::normalize(new_vert.color);
        unindexedVertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }

  // create indices
  uint32_t index_count = unindexedVertices.size();
  std::vector<uint32_t> remap(index_count);
  size_t vertex_count = meshopt_generateVertexRemap(&remap[0],
                                                    nullptr,
                                                    index_count,
                                                    unindexedVertices.data(),
                                                    index_count,
                                                    sizeof(TexturedVertex));

  indices_.resize(index_count);
  meshopt_remapIndexBuffer(indices_.data(), nullptr, index_count, remap.data());

  vertices_.resize(vertex_count);
  meshopt_remapVertexBuffer(vertices_.data(),
                            unindexedVertices.data(),
                            index_count,
                            sizeof(TexturedVertex),
                            remap.data());
}

void Mesh::optimizeMesh() {
  meshopt_optimizeVertexCache(indices_.data(), indices_.data(), indices_.size(), vertices_.size());
  //meshopt_optimizeOverdraw(indices_.data(), indices_.data(), indices_.size(), &vertices_[0].position[0], vertices_.size(), sizeof(BasicVertex), 1.05f);
  meshopt_optimizeVertexFetch(vertices_.data(),
                              indices_.data(),
                              indices_.size(),
                              vertices_.data(),
                              vertices_.size(),
                              sizeof(BasicVertex));
}

void Mesh::calcRadius() {
  radius = 0;
  for (const auto &v : vertices_) {
    radius = std::max(v.position[0]*v.position[0] + v.position[1]*v.position[1] + v.position[2]*v.position[2], radius);
  }
  radius = sqrt(radius);
}

MeshMerger &MeshMerger::addMesh(const Mesh &mesh, meshID mesh_id, vk::Pipeline pipeline, vk::PipelineLayout layout) {

  // save meshInfos
  uint32_t firstIndex = accumulated_mesh_->indices_.size();
  uint32_t indexCount = mesh.indices_.size();
  int32_t firstVertex = accumulated_mesh_->vertices_.size();
  meshInfos.emplace(mesh_id, MeshInfo{pipeline, layout, firstIndex, indexCount, firstVertex, mesh.radius});

  // insert new vertex and index data
  auto &vertex_dest = accumulated_mesh_->vertices_;
  const auto &vertex_src = mesh.vertices_;
  vertex_dest.insert(vertex_dest.end(), vertex_src.begin(), vertex_src.end());

  auto &index_dest = accumulated_mesh_->indices_;
  const auto &index_src = mesh.indices_;
  index_dest.insert(index_dest.end(), index_src.begin(), index_src.end());

  return *this;
}

MeshMerger::MeshMerger() {
  accumulated_mesh_ = std::make_unique<Mesh>();
  indexBuffer = {};
  vertexBuffer = {};
}

} //namespace rcc

