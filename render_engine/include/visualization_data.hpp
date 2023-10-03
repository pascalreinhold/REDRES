#pragma once

#include <Eigen/Dense>
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace rcc {

// big brain enum flags
// the first 8 bits are reserved for the element number
// to compare the type of atoms do:
// (tag1 << 24) == (tag2 << 24)
enum Tags : uint32_t {
  eCatalyst = 1 << 30,
  eChemical = 1 << 29,
  eHighlighted = 1 << 27,
  eSelectedForMeasurement = 1 << 26,
  eSelectedForTagging = 1 << 8
};

struct Bond {
  Bond(glm::vec3 ppos1, glm::vec3 ppos2, glm::vec3 pcolor1, glm::vec3 pcolor2) :
      pos1{ppos1}, pos2{ppos2}, color1{pcolor1}, color2{pcolor2} {
  }
  const glm::vec3 pos1;
  const glm::vec3 pos2;
  const glm::vec3 color1;
  const glm::vec3 color2;
};

struct ElementInfo {
  float atomRadius;
  glm::vec3 color;
  std::string symbol;
};

struct Event {
  int eventID = -1;
  int frameNumber = 0;
  std::vector<int> chemical_atom_numbers;
  std::vector<int> catalyst_atom_numbers;
  std::vector<int> catalyst_hinuma_indices;
  std::vector<glm::vec3> chemical_positions;
  std::vector<glm::vec3> catalyst_positions;

  glm::vec3 center{0.f};
  glm::vec3 surfaceNormal{0.f};
  glm::vec3 connectionNormal{0.f};
};

struct VisualizationData {
  //unitCell and PBC
  glm::mat3 unitCellGLM;
  Eigen::Matrix3f unitCellEigen = Eigen::Matrix3f::Zero();
  Eigen::Array3f pbcBondVector{1.f, 1.f, 1.f};

  // Hinuma
  Eigen::Matrix<float, Eigen::Dynamic, 4> hinuma_vectors;
  Eigen::Matrix<int, Eigen::Dynamic, 1> hinuma_atom_numbers;

  // Atoms
  std::vector<Eigen::MatrixX3f> positions;
  Eigen::Vector<uint32_t, Eigen::Dynamic> atomIDs;
  Eigen::Vector<uint32_t, Eigen::Dynamic> tags;
  std::map<uint32_t, ElementInfo> elementInfos;

  // Bonds
  std::vector<std::vector<Bond>> bonds;

  // Active Event
  std::unique_ptr<Event> activeEvent;

  void createBonds(float fudgeFactor);
  [[nodiscard]] Eigen::Vector3f calcMicDisplacementVec(const Eigen::Vector3f &pos1, const Eigen::Vector3f &pos2) const;

 private:
  void createBondsForNonRegularCell(float fudgeFactor);
  void createBondsForRegularCell(float fudgeFactor);
};

} // namespace rcc