//
// Created by x on 10/26/22.
//

#include "visualization_data.hpp"
#include <algorithm>
#include <execution>
#include <glm/gtx/string_cast.hpp>

namespace {
//https://gist.github.com/podgorskiy/04a3cb36a27159e296599183215a71b0
template<typename T, int m, int n>
inline glm::mat<m, n, float, glm::precision::highp> E2GLM(const Eigen::Matrix<T, m, n> &em) {
  glm::mat<m, n, float, glm::precision::highp> mat;
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      mat[j][i] = em(i, j);
    }
  }
  return mat;
}

template<typename T, int m>
inline glm::vec<m, float, glm::precision::highp> E2GLM(const Eigen::Matrix<T, m, 1> &em) {
  glm::vec<m, float, glm::precision::highp> v;
  for (int i = 0; i < m; ++i) {
    v[i] = em(i);
  }
  return v;
}
}

namespace rcc {

void VisualizationData::createBonds(const float fudgeFactor) {
  bool isDiagonal = true;
  const auto &cell = unitCellEigen;
  for (int i = 0; i < cell.rows(); i++) {
    for (int j = 0; j < cell.cols(); j++) {
      if (abs(cell(i, j)) >= 1e-4 && (i!=j)) {
        isDiagonal = false;
      }
    }
  }

  bonds.resize(positions.size());

  int estimatedBondAtomRatio = 6;
  for (int i = 0; i < bonds.size(); i++) {
    bonds[i].reserve(positions[i].rows()*estimatedBondAtomRatio);
  }
  std::cout << "Cell:\n" << unitCellEigen << "\n";
  if (isDiagonal) {
    createBondsForRegularCell(fudgeFactor);
    std::cout << "Orthorhombic Cell detected" << std::endl;
  } else {
    createBondsForNonRegularCell(fudgeFactor);
    std::cout << "Non Orthorhombic Cell detected" << std::endl;
  }

  for (auto &bond : bonds) {
    bond.shrink_to_fit();
  }

}

void VisualizationData::createBondsForNonRegularCell(const float fudgeFactor) {
  unitCellEigen.transposeInPlace();
  const Eigen::Matrix3f inverseCellMatrix = unitCellEigen.inverse();

  float maxAtomRadius = 0;
  for (const auto &elementInfo : elementInfos) {
    maxAtomRadius = std::max(maxAtomRadius, elementInfo.second.atomRadius);
  }
  const float squaredFudgeFactor = fudgeFactor*fudgeFactor;
  const float cutOff = squaredFudgeFactor*2.0f*maxAtomRadius;

  std::for_each(std::execution::par_unseq, bonds.begin(), bonds.end(), [&](const std::vector<Bond> &frame_bonds) {
    const size_t i = &frame_bonds - &bonds[0];
    const auto &framePositions = positions[i];
    std::vector<Bond> &frameBonds = bonds[i];

    for (uint32_t j = 0; j < framePositions.rows(); j++) {
      for (uint32_t k = j + 1; k < framePositions.rows(); k++) {

        const Eigen::Vector3f s_j = inverseCellMatrix*framePositions.row(j).transpose();
        const Eigen::Vector3f s_k = inverseCellMatrix*framePositions.row(k).transpose();
        Eigen::Vector3f s_jk = s_j - s_k;
        const Eigen::Vector3f s_jk_mod = rint(s_jk.array()).matrix();

        s_jk = s_jk - s_jk_mod;
        const Eigen::Vector3f r_ij = unitCellEigen*s_jk;
        const float squaredDistance = r_ij.squaredNorm();

        if (squaredDistance < cutOff) {
          auto const &elmInfo1 = elementInfos[tags(j) & 255];
          auto const &elmInfo2 = elementInfos[tags(k) & 255];
          if (squaredDistance < squaredFudgeFactor*(elmInfo1.atomRadius + elmInfo2.atomRadius)) {
            glm::vec3 pos1{framePositions.row(j)(0), framePositions.row(j)(1), framePositions.row(j)(2)};
            glm::vec3 pos2{framePositions.row(j)(0) - r_ij(0), framePositions.row(j)(1) - r_ij(1),
                           framePositions.row(j)(2) - r_ij(2)};
            frameBonds.emplace_back(pos1, pos2, elmInfo1.color, elmInfo2.color);
          }
        }
      }
    }
  });
}

// calc displacement vector between two atoms with mic
Eigen::Vector3f VisualizationData::calcMicDisplacementVec(const Eigen::Vector3f &pos1, const Eigen::Vector3f &pos2) const {
  Eigen::Matrix3f inverseCellMatrix = unitCellEigen.inverse();
  const Eigen::Vector3f s_j = inverseCellMatrix*pos1;
  const Eigen::Vector3f s_k = inverseCellMatrix*pos2;
  Eigen::Vector3f s_jk = s_j - s_k;
  const Eigen::Vector3f s_jk_mod = rint(s_jk.array()).matrix();
  s_jk = s_jk - s_jk_mod;
  return unitCellEigen*s_jk;
}

void VisualizationData::createBondsForRegularCell(const float fudgeFactor) {
  Eigen::Array3f BoxLengths = {unitCellEigen(0, 0), unitCellEigen(1, 1), unitCellEigen(2, 2)};

  float maxAtomRadius = 0;
  for (const auto &elementInfo : elementInfos) {
    maxAtomRadius = std::max(maxAtomRadius, elementInfo.second.atomRadius);
  }

  const float squaredFudgeFactor = fudgeFactor*fudgeFactor;
  float cutOff = squaredFudgeFactor*2.f*maxAtomRadius;

  std::for_each(std::execution::par_unseq, bonds.begin(), bonds.end(), [&](const std::vector<Bond> &frame_bonds) {

    const size_t i = &frame_bonds - &bonds[0];
    const auto &framePositions = positions[i];
    std::vector<Bond> &frameBonds = bonds[i];

    for (uint32_t j = 0; j < framePositions.rows(); j++) {
      for (uint32_t k = j + 1; k < framePositions.rows(); k++) {

        Eigen::Array3f r_ij = framePositions.row(j) - framePositions.row(k);
        r_ij = r_ij - rint(r_ij/BoxLengths*pbcBondVector)*BoxLengths;
        const float squaredDistance = r_ij.matrix().squaredNorm();

        if (squaredDistance < cutOff) {
          auto const &elmInfo1 = elementInfos[tags(j) & 255];
          auto const &elmInfo2 = elementInfos[tags(k) & 255];
          if (squaredDistance < squaredFudgeFactor*(elmInfo1.atomRadius + elmInfo2.atomRadius)) {
            const glm::vec3 pos1{framePositions.row(j)(0), framePositions.row(j)(1), framePositions.row(j)(2)};
            const glm::vec3 pos2{framePositions.row(j)(0) - r_ij(0), framePositions.row(j)(1) - r_ij(1),
                                 framePositions.row(j)(2) - r_ij(2)};
            frameBonds.emplace_back(pos1, pos2, elmInfo1.color, elmInfo2.color);
          }
        }
      }
    }
  });
}

} // namespace rcc