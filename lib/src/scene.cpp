//
// Created by x on 1/31/23.
//

#include "scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vector_angle.hpp>
#include "engine.hpp"

namespace rcc {

Scene::Scene(const std::string &dbFilepath, int experimentID) {
  if (!dbFilepath.empty()) visManager = std::make_unique<VisDataManager>(dbFilepath, experimentID);

  objectTypes[meshID::eAtom] = new AtomType(*this);
  objectTypes[meshID::eUnitCell] = new UnitCellType(*this);
  objectTypes[meshID::eVector] = new VectorType(*this);
  objectTypes[meshID::eCylinder] = new CylinderType(*this);
  objectTypes[meshID::eBond] = new BondType(*this);

  objectTypes[meshID::eVector]->shown = false;
  objectTypes[meshID::eCylinder]->shown = false;

  auto catalyst_color = Engine::getConfig()["CatalystColor"].get<std::array<float, 4>>();
  auto chemical_color = Engine::getConfig()["ChemicalColor"].get<std::array<float, 4>>();
  gConfig = VisualizationConfig{Engine::getConfig()["AtomSize"].get<float>(),
                                Engine::getConfig()["BondLength"].get<float>(),
                                Engine::getConfig()["BondThickness"].get<float>(),
                                Engine::getConfig()["HinumaLength"].get<float>(),
                                Engine::getConfig()["HinumaThickness"].get<float>(),
                                Engine::getConfig()["BoxCountX"].get<int>(),
                                Engine::getConfig()["BoxCountY"].get<int>(),
                                Engine::getConfig()["BoxCountZ"].get<int>(),
                                {catalyst_color[0], catalyst_color[1], catalyst_color[2], catalyst_color[3]},
                                {chemical_color[0], chemical_color[1], chemical_color[2], chemical_color[3]}
                                };

  if (visManager) freezeAtomIndex = tryPickFreezeAtom();
}

Scene::~Scene() {
  for (auto &elm : objectTypes) {
    delete elm;
  }
}

inline glm::vec3 Scene::antiStutterOffset(uint32_t movieFrameIndex) const {
  if (visManager) {
    Eigen::Vector3f temp = (freezeAtomIndex==-1) ? Eigen::Vector3f::Zero() : Eigen::Vector3f(
        visManager->data().positions[0].row(freezeAtomIndex)
            - visManager->data().positions[movieFrameIndex].row(freezeAtomIndex));
    return {temp(0), temp(1), temp(2)};
  } else {
    return {0.f, 0.f, 0.f};
  }
}

uint32_t Scene::uniqueShownObjectCount(uint32_t movieFrameIndex) const {
  uint32_t c = 0;
  for (const auto &type : objectTypes)
    if (type->shown && type->isLoaded()) {
      c += type->Count(movieFrameIndex);
    }
  return c;
}

//objectIndex means the index in the object and instance buffer
std::string Scene::getObjectInfo(uint32_t movieFrameIndex, uint32_t objectIndex) const {
  assert(objectIndex >= 0);

  for (const auto &type : objectTypes) {
    if (objectIndex < type->Count(movieFrameIndex)) return type->ObjectInfo(movieFrameIndex, objectIndex);
    objectIndex -= type->Count(movieFrameIndex);
  }
  return "Could not find corresponding Object + Type";
}

void Scene::writeObjectAndInstanceBuffer(GPUObjectData *objectSSBO,
                                         GPUInstance *instanceSSBO,
                                         uint32_t movieFrameIndex,
                                         uint32_t selectedObjectIndex) const {

  const auto &atom_positions = visManager->data().positions[movieFrameIndex];
  uint32_t object_index = 0;
  for (const auto &type : objectTypes) {
    if (type->isLoaded() && type->shown) {
      type->writeToObjectBufferAndIndexBuffer(movieFrameIndex,
                                              object_index,
                                              selectedObjectIndex,
                                              objectSSBO,
                                              instanceSSBO);
      object_index += type->Count(movieFrameIndex);
    }
  }

}
int Scene::tryPickFreezeAtom() const {
  size_t i1 = 0, i2 = (MovieFrameCount() - 1)/2, i3 = MovieFrameCount() - 1;

  // we need at least 3 frames with minimum two atoms
  if (visManager->data().positions.size() < 3 || objectTypes[meshID::eAtom]->Count(i1) < 2) { return -1; }

  const Eigen::Matrix<float, Eigen::Dynamic, 3> &pos1 = visManager->data().positions[i1];
  const Eigen::Matrix<float, Eigen::Dynamic, 3> &pos2 = visManager->data().positions[i2];
  const Eigen::Matrix<float, Eigen::Dynamic, 3> &pos3 = visManager->data().positions[i3];

  for (int i = 0; i < pos1.rows() - 1; i++) {
    if (((pos1.row(i) - pos1.row(i + 1))
        ==(pos2.row(i) - pos2.row(i + 1)))
        || ((pos2.row(i) - pos2.row(i + 1))
            ==(pos3.row(i) - pos3.row(i + 1)))) { return i; }
  }
  return -1;
}

std::string AtomType::ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const {
  std::ostringstream str;
  const auto selected_pos = s.visManager->data().positions[movieFrameIndex].row(inTypeIndex);
  std::string symbol = s.visManager->data().elementInfos.find(s.visManager->data().tags[inTypeIndex] & 255)->second.symbol;
  str << "Atom ID: " << s.visManager->data().atomIDs[inTypeIndex] << "\tSymbol: " << symbol
      << "\nAtom Coords:\t" << "[" << selected_pos(0) << ", " << selected_pos(1) << ", " << selected_pos(2) << "]";
  return str.str();
}

std::string UnitCellType::ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const {
  std::ostringstream str;
  const auto &cell = s.visManager->data().unitCellEigen;
  str << "Unit Cell ID: " << inTypeIndex << "\nUnit Cell Basis:\n"
      << "[" << cell(0, 0) << ", " << cell(0, 1) << ", " << cell(0, 2) << "]\n"
      << "[" << cell(1, 0) << ", " << cell(1, 1) << ", " << cell(1, 2) << "]\n"
      << "[" << cell(2, 0) << ", " << cell(2, 1) << ", " << cell(2, 2) << "]";
  return str.str();
}

std::string VectorType::ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const {
  std::ostringstream str;
  uint32_t hinuma_atom_id = s.visManager->data().hinuma_atom_numbers[inTypeIndex];
  Eigen::Vector4f hinuma_vec = s.visManager->data().hinuma_vectors.row(inTypeIndex);

  str << "Vector ID: " << inTypeIndex << " Attached Atom ID: " << hinuma_atom_id << "\n" <<
      "Hinuma Vector: [" << hinuma_vec(0) << ", " << hinuma_vec(1) << ", " << hinuma_vec(2) << "]\nMagnitude: "
      << hinuma_vec(3) << "\n";
  return str.str();
}

// COUNTS
uint32_t AtomType::Count(uint32_t movieFrameIndex) const {
  return s.visManager->data().positions[movieFrameIndex].rows();
}

uint32_t UnitCellType::Count(uint32_t movieFrameIndex) const {
  return 1;
}

uint32_t CylinderType::Count(uint32_t movieFrameIndex) const {
  return 1;
}

uint32_t VectorType::Count(uint32_t movieFrameIndex) const {
  return s.visManager->data().hinuma_atom_numbers.size();
}

uint32_t BondType::Count(uint32_t movieFrameIndex) const {
  return s.visManager->data().bonds[movieFrameIndex].size();
}

// MAX COUNTS
uint32_t AtomType::MaxCount() const {
  return std::max_element(s.visManager->data().positions.begin(), s.visManager->data().positions.end(),
                          [](const auto &a, const auto &b) { return a.rows() < b.rows(); })->rows();
}

uint32_t UnitCellType::MaxCount() const {
  return 1;
}

uint32_t VectorType::MaxCount() const {
  return s.visManager->data().hinuma_atom_numbers.size();
}

uint32_t CylinderType::MaxCount() const {
  return 1;
}

uint32_t BondType::MaxCount() const {
  const auto largestVectorElement = std::max_element(s.visManager->data().bonds.begin(), s.visManager->data().bonds.end(),
                                                     [](const auto &a, const auto &b) { return a.size() < b.size(); });
  return largestVectorElement->size();
}

// IS LOADED
bool AtomType::isLoaded() const {
  return !s.visManager->data().positions.empty();
}

bool BondType::isLoaded() const {
  return !s.visManager->data().bonds.empty();
}

bool VectorType::isLoaded() const {
  return s.visManager->data().hinuma_atom_numbers.size()!=0;
}

bool UnitCellType::isLoaded() const {
  return s.visManager->data().unitCellEigen!=Eigen::Matrix3f::Zero();
}


// WRITE BUFFER

//void AtomType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex, uint32_t firstIndex, uint32_t selectedObjectIndex, GPUObjectData * objectSSBO, GPUInstance* instanceSSBO) const {
//  uint32_t object_index = firstIndex;
//  const auto &atom_positions = s.visLoader->data().positions[movieFrameIndex];
//  glm::vec3 anti_stutter_offset = s.antiStutterOffset(movieFrameIndex);
//
//  // WRITE ATOM DATA
//  for (const auto &[elementNumber, elmInfo] : s.visLoader->data().elementInfos) {
//    const float &radius = elmInfo.atomRadius;
//    for (uint32_t i = 0; i < elmInfo.atomCount; i++) {
//      const glm::vec3 pos = glm::vec3{atom_positions(object_index, 0), atom_positions(object_index, 1), atom_positions(object_index, 2)} + anti_stutter_offset;
//      const auto modelMatrix = glm::translate(glm::mat4{1.f}, pos);
//      objectSSBO[object_index].modelMatrix = glm::scale(modelMatrix, glm::vec3(radius*s.gConfig.atomSize));
//
//      if(object_index == selectedObjectIndex){
//        objectSSBO[object_index].color1 = glm::vec4(1.f, 0, 0, 1.f);
//      } else if((s.visLoader->data().tags[object_index] & Tags::eSelectedByArea) == Tags::eSelectedByArea) {
//        objectSSBO[object_index].color1 = glm::vec4(1.f, 0.7f, 0.7f, 1.f);
//      } else {
//        objectSSBO[object_index].color1 = ((s.visLoader->data().tags(object_index) & Tags::eHighlighted) != Tags::eHighlighted) ? glm::vec4(elmInfo.color, 1.f) : glm::vec4(0.9f, 0.4f, 0.8f, 1.f);
//      }
//
//      objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eAtom].radius*radius*s.gConfig.atomSize;
//      objectSSBO[object_index].batchID = meshID::eAtom;
//      instanceSSBO[object_index].object_id = object_index;
//      instanceSSBO[object_index].batch_id = meshID::eAtom;
//
//      object_index++;
//    }
//  }
//}

glm::vec4 Scene::getAtomColor(uint32_t tag) const {
  // C++ syntax for calling a member function pointer pls
  return (this ->* ((const rcc::Scene*)this)->rcc::Scene::current_atom_color_function)(tag);
}

glm::vec4 Scene::colorAtomByElementNumber(uint32_t tag) const {
  if ((tag & Tags::eSelectedForMeasurement)==Tags::eSelectedForMeasurement) return {0.224f, 1.f, 0.078f, 1.f};
  if ((tag & Tags::eSelectedForTagging)==Tags::eSelectedForTagging) return {0.7f, 0.72f, 0.95f, 1.f};
  if ((tag & Tags::eHighlighted)==Tags::eHighlighted) return {0.83f, 0.1f, 0.7f, 1.f};
  return {visManager->data().elementInfos.find((tag & 255))->second.color, 1.f};
}

glm::vec4 Scene::colorAtomByBaseType(uint32_t tag) const {
  if ((tag & Tags::eSelectedForMeasurement)==Tags::eSelectedForMeasurement) return {0.224f, 1.f, 0.078f, 1.f};
  if ((tag & Tags::eSelectedForTagging)==Tags::eSelectedForTagging) return {0.7f, 0.72f, 0.95f, 1.f};
  if ((tag & Tags::eHighlighted)==Tags::eHighlighted) return {0.83f, 0.1f, 0.7f, 1.f};
  if ((tag & Tags::eCatalyst)==Tags::eCatalyst) return gConfig.catalyst_color_;
  if ((tag & Tags::eChemical)==Tags::eChemical) return gConfig.chemical_color_;
  return {visManager->data().elementInfos.find((tag & 255))->second.color, 1.f};
}

void AtomType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                                 uint32_t firstIndex,
                                                 uint32_t selectedObjectIndex,
                                                 GPUObjectData *objectSSBO,
                                                 GPUInstance *instanceSSBO) const {
  uint32_t object_index = firstIndex;
  const auto &atom_positions = s.visManager->data().positions[movieFrameIndex];
  glm::vec3 anti_stutter_offset = s.antiStutterOffset(movieFrameIndex);

  for (int i = 0; i < atom_positions.rows(); i++) {
    uint32_t element_number = (s.visManager->data().tags[object_index] & 255);
    const float radius = s.visManager->data().elementInfos.find(element_number)->second.atomRadius;
    const glm::vec3 pos =
        glm::vec3{atom_positions(object_index, 0), atom_positions(object_index, 1), atom_positions(object_index, 2)}
            + anti_stutter_offset;
    const auto modelMatrix = glm::translate(glm::mat4{1.f}, pos);
    objectSSBO[object_index].modelMatrix = glm::scale(modelMatrix, glm::vec3(radius*s.gConfig.atomSize));
    objectSSBO[object_index].color1 = s.getAtomColor(s.visManager->data().tags[object_index]);
    objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eAtom].radius*radius*s.gConfig.atomSize;
    objectSSBO[object_index].batchID = meshID::eAtom;
    instanceSSBO[object_index].object_id = object_index;
    instanceSSBO[object_index].batch_id = meshID::eAtom;
    object_index++;
  }

}

void UnitCellType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                                     uint32_t firstIndex,
                                                     uint32_t selectedObjectIndex,
                                                     rcc::GPUObjectData *objectSSBO,
                                                     rcc::GPUInstance *instanceSSBO) const {
  uint32_t object_index = firstIndex;

  // WRITE UNIT CELL DATA
  objectSSBO[object_index].modelMatrix = glm::mat4{1.0f};
  objectSSBO[object_index].color1 = (selectedObjectIndex != s["Atom"].Count(movieFrameIndex)) ? glm::vec4(1.f) : glm::vec4(0.f, 1.f, 0.f, 1.f);
  objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eUnitCell].radius;
  objectSSBO[object_index].batchID = meshID::eUnitCell;
  instanceSSBO[object_index].object_id = object_index;
  instanceSSBO[object_index].batch_id = meshID::eUnitCell;

}

void VectorType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                                   uint32_t firstIndex,
                                                   uint32_t selectedObjectIndex,
                                                   GPUObjectData *objectSSBO,
                                                   GPUInstance *instanceSSBO) const {
  uint32_t object_index = firstIndex;
  const auto &atom_positions = s.visManager->data().positions[movieFrameIndex];
  glm::vec3 anti_stutter_offset = s.antiStutterOffset(movieFrameIndex);

  for (int i = 0; i < s.visManager->data().hinuma_atom_numbers.size(); i++) {
    const Eigen::Matrix<int, Eigen::Dynamic, 1> &id_vec = s.visManager->data().hinuma_atom_numbers;
    const Eigen::Matrix<float, Eigen::Dynamic, 4> &vectors = s.visManager->data().hinuma_vectors;

    auto const &elm_info = s.visManager->data().elementInfos.find(s.visManager->data().tags(id_vec[i]) & 255)->second;

    const float length = vectors(i, 3);
    const glm::vec3 hinuma_vec = normalize(glm::vec3{vectors(i, 0), vectors(i, 1), vectors(i, 2)});
    const glm::vec3 unit_vec{0.f, 1.f, 0.f};

    const glm::vec3
        pos = glm::vec3{atom_positions(id_vec[i], 0), atom_positions(id_vec[i], 1), atom_positions(id_vec[i], 2)}
        + hinuma_vec*elm_info.atomRadius*s.gConfig.atomSize + anti_stutter_offset;

    auto modelMatrix = glm::translate(glm::mat4{1.f}, pos);
    const auto rotationAxis = glm::cross(unit_vec, hinuma_vec);
    const float angle = glm::orientedAngle(unit_vec, hinuma_vec, rotationAxis);
    modelMatrix = glm::rotate(modelMatrix, angle, rotationAxis);

    objectSSBO[object_index].modelMatrix = glm::scale(modelMatrix, glm::vec3(
        s.gConfig.hinumaVectorThickness, length*s.gConfig.hinumaVectorLength, s.gConfig.hinumaVectorThickness));

    objectSSBO[object_index].color1 =
        (object_index!=selectedObjectIndex) ? glm::vec4(1, 0, 0, 1.f) : glm::vec4(0, 1, 0, 1.f);
    objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eVector].radius*length*s.gConfig.hinumaVectorLength;
    objectSSBO[object_index].batchID = meshID::eVector;

    instanceSSBO[object_index].object_id = object_index;
    instanceSSBO[object_index].batch_id = meshID::eVector;

    object_index++;
  }
}

void BondType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                                 uint32_t firstIndex,
                                                 uint32_t selectedObjectIndex,
                                                 GPUObjectData *objectSSBO,
                                                 GPUInstance *instanceSSBO) const {
  assert(isLoaded());
  uint32_t object_index = firstIndex;
  glm::vec3 anti_stutter_offset = s.antiStutterOffset(movieFrameIndex);
  const auto &bonds = s.visManager->data().bonds[movieFrameIndex];
  for (const auto &bond : bonds) {
    const glm::vec3 pos = (bond.pos1 + bond.pos2)*0.5f;
    const glm::vec3 displacement = bond.pos1 - bond.pos2;
    const glm::vec3 cylinder = glm::vec3(0.f, 1.f, 0.f);
    const glm::vec3 rotationAxis = glm::cross(displacement, cylinder);

    const float length = glm::l2Norm(displacement);
    const float angle = acosf(-glm::dot(displacement, cylinder)/length);

    objectSSBO[object_index].color1 = glm::vec4(bond.color1, 1.f);
    objectSSBO[object_index].color2 = glm::vec4(bond.color2, 1.f);
    objectSSBO[object_index].bond_normal = glm::vec4(displacement, 0);
    auto model_matrix = glm::translate(glm::mat4{1.0}, pos + anti_stutter_offset);
    model_matrix = glm::rotate(model_matrix, angle, rotationAxis);
    model_matrix = glm::scale(model_matrix,
                              glm::vec3(s.gConfig.bondThickness,
                                        length/2.f*s.gConfig.bondLength,
                                        s.gConfig.bondThickness));
    objectSSBO[object_index].modelMatrix = model_matrix;
    objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eBond].radius*(length/2.f)*s.gConfig.bondLength;
    objectSSBO[object_index].batchID = meshID::eBond;

    instanceSSBO[object_index].object_id = object_index;
    instanceSSBO[object_index].batch_id = meshID::eBond;
    object_index++;
  }
}

void CylinderType::writeToObjectBufferAndIndexBuffer(uint32_t movieFrameIndex,
                                                     uint32_t firstIndex,
                                                     uint32_t selectedObjectIndex,
                                                     GPUObjectData *objectSSBO,
                                                     GPUInstance *instanceSSBO) const {
  uint32_t object_index = firstIndex;

  const auto &atom_positions = s.visManager->data().positions[movieFrameIndex];
  glm::vec3 anti_stutter_offset = s.antiStutterOffset(movieFrameIndex);

  const auto &activeEvent = s.visManager->data().activeEvent;
  if (activeEvent!=nullptr) {
    glm::vec3 n = s.eventViewerSettings.surfaceNormals ? activeEvent->surfaceNormal : activeEvent->connectionNormal;
    glm::vec3 cylinder_up{0.f, 1.f, 0.f};
    auto modelMatrix = glm::translate(glm::mat4{1.f}, activeEvent->center);
    const auto rotationAxis = glm::cross(cylinder_up, n);
    const float angle = glm::orientedAngle(cylinder_up, n, rotationAxis);
    modelMatrix = glm::rotate(modelMatrix, angle, rotationAxis);

    /*
    // rotate cylinder towards camera
    glm::vec3 transformedCylinderUp = glm::mat3(modelMatrix) * glm::vec3{0.f, 0.f, 1.f};
    glm::vec3 transformedRight = glm::mat3(modelMatrix) * glm::vec3{1.f, 0.f, 0.f};
    const float angleToCam = glm::orientedAngle(camera_view_direction, transformedRight, transformedCylinderUp);
    modelMatrix = glm::rotate(modelMatrix, static_cast<float>(angleToCam), transformedCylinderUp);

    static int test = 0;
    test++;
    if(test%30==0){
      std::cout << "Angle to Cam: " << angleToCam << std::endl;
    }
    */

    // write to object buffer
    objectSSBO[object_index].modelMatrix = glm::scale(modelMatrix, glm::vec3(
        s.eventViewerSettings.cylinderRadius*2.f,
        s.eventViewerSettings.cylinderLength,
        s.eventViewerSettings.cylinderRadius*2.f));

  } else {
    objectSSBO[object_index].modelMatrix = glm::translate(glm::mat4{1.0f}, anti_stutter_offset);
  }
  objectSSBO[object_index].color1 = glm::vec4(1.f);
  objectSSBO[object_index].radius = s.meshes->meshInfos[meshID::eCylinder].radius
      *std::max(s.eventViewerSettings.cylinderLength, s.eventViewerSettings.cylinderRadius*2.f);
  objectSSBO[object_index].batchID = meshID::eCylinder;
  instanceSSBO[object_index].object_id = object_index;
  instanceSSBO[object_index].batch_id = meshID::eCylinder;
}

bool CylinderType::isLoaded() const {
  return true;
}

std::string CylinderType::ObjectInfo(uint32_t movieFrameIndex, uint32_t inTypeIndex) const {
  return "";
}

} //namespace rcc