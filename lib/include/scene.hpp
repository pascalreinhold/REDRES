//
// Created by x on 1/31/23.
//

#pragma once

#include "visualization_data.hpp"
#include "mesh.hpp"
#include "utils.hpp"
#include "visualization_data_loader.hpp"
#include <cstdint>
#include <utility>

namespace rcc {

class Scene;

struct ObjectType {
  ObjectType(const Scene &scene, std::string identifier, meshID mID) : s{scene},
                                                                       typeIdentifier{std::move(identifier)},
                                                                       mesh_id(mID) {};
  virtual ~ObjectType() = default;
  [[nodiscard]] virtual std::string ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const { return ""; };
  [[nodiscard]] virtual uint32_t Count(uint32_t movieFrameIndex) const = 0;
  [[nodiscard]] virtual uint32_t MaxCount() const = 0;
  [[nodiscard]] virtual bool isLoaded() const = 0;
  virtual void writeToObjectBufferAndIndexBuffer(
      uint32_t movieFrameIndex,
      uint32_t firstIndex,
      uint32_t selectedObjectIndex,
      GPUObjectData *objectSSBO,
      GPUInstance *instanceSSBO) const = 0;

  std::string typeIdentifier;
  const Scene &s;
  meshID mesh_id = eAtom;
  bool shown = true;
};

struct AtomType : public ObjectType {
  explicit AtomType(const Scene &scene) : ObjectType(scene, "Atom", meshID::eAtom) {}
  void writeToObjectBufferAndIndexBuffer(
      uint32_t movieFrameIndex,
      uint32_t firstIndex,
      uint32_t selectedObjectIndex,
      GPUObjectData *objectSSBO,
      GPUInstance *instanceSSBO) const override;
  [[nodiscard]] std::string ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const override;
  [[nodiscard]] uint32_t Count(uint32_t movieFrameIndex) const override;
  [[nodiscard]] uint32_t MaxCount() const override;
  [[nodiscard]] bool isLoaded() const override;
};

struct BondType : public ObjectType {
  explicit BondType(const Scene &scene) : ObjectType(scene, "Bond", meshID::eBond) {};
  void writeToObjectBufferAndIndexBuffer(
      uint32_t movieFrameIndex,
      uint32_t firstIndex,
      uint32_t selectedObjectIndex,
      GPUObjectData *objectSSBO,
      GPUInstance *instanceSSBO) const override;
  [[nodiscard]] uint32_t Count(uint32_t movieFrameIndex) const override;
  [[nodiscard]] uint32_t MaxCount() const override;
  [[nodiscard]] bool isLoaded() const override;
};

struct VectorType : public ObjectType {
  explicit VectorType(const Scene &scene) : ObjectType(scene, "Vector", meshID::eVector) {};
  [[nodiscard]] uint32_t Count(uint32_t movieFrameIndex) const override;
  [[nodiscard]] uint32_t MaxCount() const override;
  [[nodiscard]] bool isLoaded() const override;
  void writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                         uint32_t firstIndex,
                                         uint32_t selectedObjectIndex,
                                         GPUObjectData *objectSSBO,
                                         GPUInstance *instanceSSBO) const override;
  [[nodiscard]] std::string ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const override;
};

struct UnitCellType : public ObjectType {
  explicit UnitCellType(const Scene &scene) : ObjectType(scene, "UnitCell", meshID::eUnitCell) {}
  [[nodiscard]] std::string ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const override;
  [[nodiscard]] uint32_t Count(uint32_t movieFrameIndex) const override;
  [[nodiscard]] uint32_t MaxCount() const override;
  [[nodiscard]] bool isLoaded() const override;
  void writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                         uint32_t firstIndex,
                                         uint32_t selectedObjectIndex,
                                         GPUObjectData *objectSSBO,
                                         GPUInstance *instanceSSBO) const override;
};

struct CylinderType : public ObjectType {
  explicit CylinderType(const Scene &scene) : ObjectType(scene, "Cylinder", meshID::eCylinder) {}
  [[nodiscard]] std::string ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const override;
  [[nodiscard]] uint32_t Count(uint32_t movieFrameIndex) const override;
  [[nodiscard]] uint32_t MaxCount() const override;
  [[nodiscard]] bool isLoaded() const override;
  void writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                         uint32_t firstIndex,
                                         uint32_t selectedObjectIndex,
                                         GPUObjectData *objectSSBO,
                                         GPUInstance *instanceSSBO) const override;
  glm::vec3 camera_view_direction{0.f, 0.f, -1.f};
};

class Scene {
 public:
  explicit Scene(const std::string &dbFilepath, int experimentID);
  ~Scene();

  // init this from settings file in constructor
  struct VisualizationConfig {
    float atomSize;
    float bondLength;
    float bondThickness;
    float hinumaVectorLength;
    float hinumaVectorThickness;
    int xCellCount;
    int yCellCount;
    int zCellCount;
    glm::vec4 catalyst_color_;
    glm::vec4 chemical_color_;
  } gConfig{};

  ObjectType *objectTypes[RCC_MESH_COUNT]{};
  void setMeshes(MeshMerger *mesh_merger) { meshes = mesh_merger; };

  [[nodiscard]] uint32_t MovieFrameCount() const { return visManager->data().positions.size(); }
  [[nodiscard]] std::string getObjectInfo(uint32_t movieFrameIndex, uint32_t objectIndex) const;
  [[nodiscard]] uint32_t uniqueShownObjectCount(uint32_t movieFrameIndex) const;
  [[nodiscard]] const glm::mat3 &cellGLM() const { return visManager->data().unitCellGLM; }
  [[nodiscard]] const Eigen::Matrix<float, 3, 3> &cellEigen() const { return visManager->data().unitCellEigen; }
  [[nodiscard]] int freezeAtom() const { return freezeAtomIndex; }
  [[nodiscard]] int tryPickFreezeAtom() const; // returns -1 if not successful
  [[nodiscard]] inline glm::vec3 antiStutterOffset(uint32_t movieFrameIndex) const;

  void pickFreezeAtom(const int atomIndex) { freezeAtomIndex = atomIndex; }
  void writeObjectAndInstanceBuffer(GPUObjectData *objectData,
                                    GPUInstance *instanceData,
                                    uint32_t movieFrameIndex,
                                    uint32_t selectedObjectIndex) const;

  ObjectType &operator[](const std::string &identifier) const {
    for (auto type : objectTypes) {
      if (type->typeIdentifier==identifier) return *type;
    }
    abort();
  }

  std::unique_ptr<VisDataManager> visManager;

  struct {
    float cylinderLength = 11.f;
    float cylinderRadius = 8.f;
    bool enableCylinderCulling = true;
    bool surfaceNormals = true;
  } eventViewerSettings;

  MeshMerger *meshes = nullptr;
 private:
  int freezeAtomIndex = -1;

  [[nodiscard]] glm::vec4 getAtomColor(uint32_t tag) const;
  [[nodiscard]] glm::vec4 colorAtomByElementNumber(uint32_t tag) const;
  [[nodiscard]] glm::vec4 colorAtomByBaseType(uint32_t tag) const;
  // function pointer to atom color member function
  glm::vec4 (Scene::*current_atom_color_function)(uint32_t tag) const = &Scene::colorAtomByElementNumber;
 public:
  void activateColorByElementNumber() { current_atom_color_function = &Scene::colorAtomByElementNumber; }
  void activateColorByBaseType() { current_atom_color_function = &Scene::colorAtomByBaseType; }

  friend AtomType;
  friend UnitCellType;
  friend VectorType;
  friend CylinderType;
  friend BondType;
};

} // namespace rcc