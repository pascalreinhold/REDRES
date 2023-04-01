//
// Created by x on 3/1/23.
//


#include "visualization_data_loader.hpp"
#include "engine.hpp"
#include <Eigen/StdVector>
#include <chrono>
#include <execution>

#ifdef RCC_ACTIVATE_PRINT_TIMING
#define RCC_PRINT_TIMING(code)\
    do{ \
    auto start = std::chrono::steady_clock::now(); \
    code; \
    auto end = std::chrono::steady_clock::now(); \
    std::chrono::duration<double> elapsed_seconds = end - start; \
    std::cout << "elapsed time for " << #code << ": " << elapsed_seconds.count() * 1000.f << "ms\n"; \
    } while (false);
#else
#define RCC_PRINT_TIMING(code)\
  code;
#endif

namespace {
bool sqlCheck(int result) {
  if (result!=SQLITE_OK) {
    std::cerr << "ALARM\n";
  }
  return result==SQLITE_OK;
}
}

namespace rcc {

VisDataManager::VisDataManager(const std::string &db_filepath, const int experiment_id) : db_filepath_(db_filepath) {
  std::cout << "sqlite version: " << sqlite3_version << "\n";
  std::cout << "DB-Filepath: " << db_filepath_ << "\n";
  if (sqlCheck(sqlite3_open_v2(db_filepath_.c_str(), &db, SQLITE_OPEN_READWRITE , nullptr))) {
    std::cout << "successfully connected to " << db_filepath_ << std::endl;
  } else {
    std::cerr << "could not connect to" << db_filepath_ << std::endl;
  }

  auto start = std::chrono::steady_clock::now();
  load(experiment_id);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  std::cout << "elapsed time for loading experiment Nr." << experiment_id << " and creating Bonds: "
            << elapsed_seconds.count()*1000.f << "ms\n";
}

void VisDataManager::load(const int experiment_id) {
  //get system and setting ids
  vis = std::make_unique<VisualizationData>();
  sqlite3_stmt *query;
  sqlCheck(sqlite3_prepare_v2(db, "SELECT system_id, setting_id FROM experiments WHERE id = ?", -1, &query, nullptr));
  sqlite3_bind_int(query, 1, experiment_id);
  sqlite3_step(query);

  experimentID_ = experiment_id;
  systemID_ = sqlite3_column_int(query, 0);
  settingID_ = sqlite3_column_int(query, 1);
  sqlite3_finalize(query);

  RCC_PRINT_TIMING(loadUnitCell(systemID_);)
  RCC_PRINT_TIMING(loadElementInfos(systemID_);)
  RCC_PRINT_TIMING(loadAtomPositions(systemID_);)
  RCC_PRINT_TIMING(loadAtomElementNumbersAndTags(experimentID_);)
  RCC_PRINT_TIMING(loadBonds(settingID_);)
  RCC_PRINT_TIMING(loadHinuma(experiment_id);)
}

static int sqlPositionReaderCallBack(void *data, int, char **columns, char **) {
  auto *helper = reinterpret_cast<SqlPositionReaderHelper *>(data);

  int &counter = helper->indexCounter;
  const int frame = counter/helper->atomCount;
  const int positionID = counter%helper->atomCount;
  (*(helper->allPositions))[frame](positionID, 0) = strtof(columns[0], nullptr);
  (*(helper->allPositions))[frame](positionID, 1) = strtof(columns[1], nullptr);
  (*(helper->allPositions))[frame](positionID, 2) = strtof(columns[2], nullptr);
  counter++;
  return 0;
}

void VisDataManager::loadAtomElementNumbersAndTags(int experimentID) {
  sqlite3_stmt *query;

  // get chemical and catalyst base_type_ids
  sqlite3_prepare_v2(db, "SELECT id FROM base_types WHERE name=\"chemical\"", -1, &query, nullptr);
  sqlite3_step(query);
  int chemicalID = sqlite3_column_int(query, 0);
  sqlite3_prepare_v2(db, "SELECT id FROM base_types WHERE name=\"catalyst\"", -1, &query, nullptr);
  sqlite3_step(query);
  int catalystID = sqlite3_column_int(query, 0);
  sqlite3_prepare_v2(db, "SELECT id FROM properties WHERE name=\"init_base_type\"", -1, &query, nullptr);
  sqlite3_step(query);
  int propertyID = sqlite3_column_int(query, 0);



  sqlite3_prepare_v2(db,
                     "SELECT atoms.id, atoms.atom_number, atoms.atomic_number, atom_tags.value FROM atoms INNER JOIN atom_tags on atoms.id = atom_tags.atom_id AND atom_tags.property_id = ? WHERE experiment_id = ?",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, propertyID);
  sqlite3_bind_int(query, 2, experimentID);

  vis->atomIDs = Eigen::Vector<uint32_t, Eigen::Dynamic>::Zero(vis->positions[0].rows());
  vis->tags = Eigen::Vector<uint32_t, Eigen::Dynamic>::Zero(vis->positions[0].rows());

  while (sqlite3_step(query)!=SQLITE_DONE) {
    const uint32_t atom_id = sqlite3_column_int(query, 1);
    const uint32_t atom_number = sqlite3_column_int(query, 1);
    const uint32_t atomic_number = sqlite3_column_int(query, 2);
    const uint32_t base_type_id = sqlite3_column_int(query, 3);
    const uint32_t base_type_tags =
        ((base_type_id==chemicalID) ? Tags::eChemical : 0) | ((base_type_id==catalystID) ? Tags::eCatalyst : 0);

    vis->atomIDs[atom_number] = atom_id;
    vis->tags[atom_number] = (vis->tags[atom_number] | atomic_number | base_type_tags);
  }

  sqlite3_finalize(query);
}

void VisDataManager::loadAtomPositions(int systemID) {
  sqlite3_stmt *query;

  //get FrameIndices
  std::vector<int> frameIDs;
  frameIDs.reserve(2000);
  sqlite3_prepare_v2(db, "SELECT id FROM frames WHERE system_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, systemID);

  while (sqlite3_step(query)!=SQLITE_DONE) {
    frameIDs.push_back(sqlite3_column_int(query, 0));
  }
  sqlite3_finalize(query);
  frameIDs.shrink_to_fit();

  //get MaxAtomCount
  sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM atoms WHERE system_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, systemID);
  sqlite3_step(query);
  int maxAtomCount = sqlite3_column_int(query, 0);

  //resize all positions vector
  std::vector<Eigen::Matrix<float, Eigen::Dynamic, 3>> &allPositions = vis->positions;
  allPositions =
      std::vector<Eigen::MatrixX3f>(frameIDs.size(), Eigen::Matrix<float, Eigen::Dynamic, 3>(maxAtomCount, 3));
  SqlPositionReaderHelper helper{0, maxAtomCount, &allPositions};

  sqlite3_prepare_v2(db, "SELECT MIN(id) FROM positions WHERE frame_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, frameIDs[0]);
  sqlite3_step(query);
  int firstID = sqlite3_column_int(query, 0);
  sqlite3_finalize(query);

  sqlite3_prepare_v2(db, "SELECT MAX(id) FROM positions WHERE frame_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, frameIDs[frameIDs.size() - 1]);
  sqlite3_step(query);
  int lastID = sqlite3_column_int(query, 0);

  auto cmd3 = "SELECT x,y,z FROM positions WHERE id BETWEEN " + std::to_string(firstID)
      + " AND " + std::to_string(lastID) + " ORDER BY id ASC";

  sqlite3_exec(db, cmd3.c_str(), sqlPositionReaderCallBack, &helper, nullptr);
}

static glm::vec3 convertHexToRGB(uint32_t hex) {
  float r = static_cast<float>(hex >> 16)/255.f;
  float g = static_cast<float>((hex >> 8) & 255)/255.f;
  float b = static_cast<float>(hex & 255)/255.f;
  return glm::vec3{r, g, b};
}

static glm::vec3 convertHexStringToRGB(const char *hex_c_string) {
  if (!hex_c_string) return glm::vec3{0.f};
  std::string hex_string{hex_c_string};
  std::string prefix = "0x";
  hex_string.erase(hex_string.cbegin());
  hex_string.insert(0, prefix);
  return convertHexToRGB(std::stoul(hex_string, nullptr, 16));
}

void VisDataManager::loadElementInfos(int systemID) {
  // load the element numbers for the system
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db,
                     "SELECT DISTINCT atomic_number FROM atoms WHERE system_id = ? ORDER BY atom_number",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, systemID);

  sqlite3_stmt *elmInfoQuery;
  sqlite3_prepare_v2(db,
                     "SELECT chemical_symbol, covalent_radius_pyykko, cpk_color FROM elements WHERE id = ?",
                     -1,
                     &elmInfoQuery,
                     nullptr);

  while (sqlite3_step(query)!=SQLITE_DONE) {
    // get the atomic number
    int atomicNumber = sqlite3_column_int(query, 0);
    sqlite3_bind_int(elmInfoQuery, 1, atomicNumber);

    // run element info query on the atomic number
    sqlite3_step(elmInfoQuery);
    std::string elmSymbol((char *) sqlite3_column_text(elmInfoQuery, 0));
    auto radius = static_cast<float>(sqlite3_column_double(elmInfoQuery, 1));
    glm::vec3 color = convertHexStringToRGB(reinterpret_cast<const char *>(sqlite3_column_text(elmInfoQuery, 2)));
    sqlite3_reset(elmInfoQuery);

    vis->elementInfos[static_cast<uint32_t>(atomicNumber)] =
        ElementInfo{radius, color, elmSymbol};
  }
  sqlite3_finalize(query);
  sqlite3_finalize(elmInfoQuery);
}

void VisDataManager::loadHinuma(int experimentID) {
  sqlite3_stmt *query;

  sqlite3_prepare_v2(db, "SELECT id FROM hinuma WHERE experiment_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, experimentID);
  sqlite3_step(query);
  int hinumaID = sqlite3_column_int(query, 0);

  sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM hinuma_atoms WHERE hinuma_id = ?", -1, &query, nullptr);
  sqlite3_bind_int(query, 1, hinumaID);
  sqlite3_step(query);
  int hinumaAtomCount = sqlite3_column_int(query, 0);

  //resize hinuma containers
  vis->hinuma_atom_numbers.resize(hinumaAtomCount, Eigen::NoChange);
  vis->hinuma_vectors.resize(hinumaAtomCount, Eigen::NoChange);

  sqlite3_prepare_v2(db,
                     "SELECT atoms.atom_number, hinuma_vec_x, hinuma_vec_y, hinuma_vec_z, solid_angle  FROM hinuma_atoms INNER JOIN atoms ON atom_id = atoms.id WHERE hinuma_id = ?",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, hinumaID);
  int hinumaIndex = 0;
  while (sqlite3_step(query)!=SQLITE_DONE) {
    vis->hinuma_atom_numbers[hinumaIndex] = sqlite3_column_int(query, 0);
    vis->hinuma_vectors(hinumaIndex, 0) = strtof((const char *) (sqlite3_column_text(query, 1)), nullptr);
    vis->hinuma_vectors(hinumaIndex, 1) = strtof((const char *) (sqlite3_column_text(query, 2)), nullptr);
    vis->hinuma_vectors(hinumaIndex, 2) = strtof((const char *) (sqlite3_column_text(query, 3)), nullptr);
    vis->hinuma_vectors(hinumaIndex, 3) = strtof((const char *) (sqlite3_column_text(query, 4)), nullptr);
    hinumaIndex++;
  }
  sqlite3_finalize(query);
}

void VisDataManager::loadBonds(int settingID) {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db,
                     "SELECT value FROM setting_parameters WHERE setting_id = ? AND parameter_id = (SELECT id FROM parameters WHERE name = \"fudge_factor\")",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, settingID);
  sqlite3_step(query);

  float fudgeFactor = strtof((const char *) (sqlite3_column_text(query, 0)), nullptr);
  vis->createBonds(fudgeFactor);

  sqlite3_finalize(query);
}

void VisDataManager::exportSettingText(int settingID, SettingsText &settings) {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db,
                     "SELECT name, value, description FROM setting_parameters INNER JOIN parameters ON setting_parameters.parameter_id = parameters.id WHERE setting_id = ? ORDER BY parameter_id",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, settingID);

  while (sqlite3_step(query)!=SQLITE_DONE) {
    const char *name = (reinterpret_cast<const char *>(sqlite3_column_text(query, 0)));
    const char *value = (reinterpret_cast<const char *>(sqlite3_column_text(query, 1)));
    const char *description = (reinterpret_cast<const char *>(sqlite3_column_text(query, 2)));
    const std::string name_str = (name) ? std::string(name) : "";
    const std::string value_str = (value) ? std::string(value) : "";
    const std::string description_str = (description) ? std::string(description) : "";

    settings.parameters.emplace_back(std::array<std::string, 3>{name_str, value_str, description_str});
  }

  sqlite3_finalize(query);
}

void VisDataManager::exportEvents(int experimentID, EventsText &events) {
  sqlite3_stmt *queryEvents;
  sqlite3_prepare_v2(db,
                     "SELECT events.id, frame_id, event_types.name, event_types.description FROM events INNER JOIN event_types ON event_type_id = event_types.id  WHERE experiment_id = ? ORDER BY frame_id",
                     -1,
                     &queryEvents,
                     nullptr);

  sqlite3_bind_int(queryEvents, 1, experimentID);
  while (sqlite3_step(queryEvents)!=SQLITE_DONE) {
    events.events.emplace_back(
        sqlite3_column_int(queryEvents, 0),
        sqlite3_column_int(queryEvents, 1),
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(queryEvents, 2))),
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(queryEvents, 3)))
    );
  }
  sqlite3_finalize(queryEvents);
}

void VisDataManager::exportExperiments(Experiments &experiments) {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db, "SELECT id, system_id, setting_id FROM experiments", -1, &query, nullptr);

  while (sqlite3_step(query)!=SQLITE_DONE) {
    experiments.experimentSystemSettingIDs.emplace_back(sqlite3_column_int(query, 0), sqlite3_column_int(query, 1),
                                                        sqlite3_column_int(query, 2));
  }
  sqlite3_finalize(query);
}

void VisDataManager::loadUnitCell(int systemID) {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db,
                     "SELECT cell_1_x, cell_2_x, cell_3_x, cell_1_y, cell_2_y, cell_3_y, cell_1_z, cell_2_z, cell_3_z, pbc_x, pbc_y, pbc_z FROM systems WHERE id = ?",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, systemID);
  sqlite3_step(query);

  vis->unitCellEigen(0,0) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 0)), nullptr);
  vis->unitCellEigen(0,1) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 1)), nullptr);
  vis->unitCellEigen(0,2) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 2)), nullptr);
  vis->unitCellEigen(1,0) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 3)), nullptr);
  vis->unitCellEigen(1,1) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 4)), nullptr);
  vis->unitCellEigen(1,2) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 5)), nullptr);
  vis->unitCellEigen(2,0) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 6)), nullptr);
  vis->unitCellEigen(2,1) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 7)), nullptr);
  vis->unitCellEigen(2,2) = strtof(reinterpret_cast<const char *>(sqlite3_column_text(query, 8)), nullptr);

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
       vis->unitCellGLM[i][j] = vis->unitCellEigen(i, j);
    }
  }

  vis->pbcBondVector[0] = static_cast<float>(sqlite3_column_int(query, 9));
  vis->pbcBondVector[1] = static_cast<float>(sqlite3_column_int(query, 10));
  vis->pbcBondVector[2] = static_cast<float>(sqlite3_column_int(query, 11));

  sqlite3_finalize(query);
}

void VisDataManager::loadActiveEvent(int eventID) {
  sqlite3_stmt *query;
  vis->activeEvent = std::make_unique<Event>();
  vis->activeEvent->eventID = eventID;

  sqlite3_prepare_v2(db,
                     "SELECT frame_number, frames.id FROM frames INNER JOIN events ON frames.id = events.frame_id WHERE events.id = ?",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, eventID);
  sqlite3_step(query);
  vis->activeEvent->frameNumber = sqlite3_column_int(query, 0);
  const int frameID = sqlite3_column_int(query, 1);

  sqlite3_prepare_v2(db,
                     "SELECT atom_number FROM event_atoms INNER JOIN atoms on event_atoms.atom_id = atoms.id WHERE event_id = ?",
                     -1,
                     &query,
                     nullptr);
  sqlite3_bind_int(query, 1, eventID);


  while (sqlite3_step(query)!=SQLITE_DONE) {
    int atomNumber = sqlite3_column_int(query, 0);

    if ((vis->tags[atomNumber] & Tags::eCatalyst) == Tags::eCatalyst) {
      vis->activeEvent->catalyst_atom_numbers.emplace_back(atomNumber);
      glm::vec3 atomPos{vis->positions[vis->activeEvent->frameNumber](atomNumber, 0),
                        vis->positions[vis->activeEvent->frameNumber](atomNumber, 1),
                        vis->positions[vis->activeEvent->frameNumber](atomNumber, 2)};
      vis->activeEvent->catalyst_positions.emplace_back(atomPos);

    } else if  ((vis->tags[atomNumber] & Tags::eChemical) == Tags::eChemical) {
      vis->activeEvent->chemical_atom_numbers.emplace_back(atomNumber);
      glm::vec3 atomPos{vis->positions[vis->activeEvent->frameNumber](atomNumber, 0),
                        vis->positions[vis->activeEvent->frameNumber](atomNumber, 1),
                        vis->positions[vis->activeEvent->frameNumber](atomNumber, 2)};
      vis->activeEvent->chemical_positions.emplace_back(atomPos);
    } else {
      std::cout << "event atom is neither chemical or catalyst?" << std::endl;
    }
  }

  vis->activeEvent->center = glm::vec3{0.0f};
  for (auto &catalystPosition : vis->activeEvent->catalyst_positions) {
    vis->activeEvent->center += catalystPosition;
  }
  vis->activeEvent->center /= vis->activeEvent->catalyst_positions.size();

  for (int j = 0; j < vis->activeEvent->catalyst_atom_numbers.size(); j++) {
    for (int i = 0; i < vis->hinuma_atom_numbers.rows(); i++) {
      if (vis->hinuma_atom_numbers[i]==vis->activeEvent->catalyst_atom_numbers[j]) {
        vis->activeEvent->catalyst_hinuma_indices.push_back(i);
      }
    }
  }

  int hinumaIndex = vis->activeEvent->catalyst_hinuma_indices[0];
  vis->activeEvent->surfaceNormal =
      glm::normalize(glm::vec3{vis->hinuma_vectors(hinumaIndex, 0), vis->hinuma_vectors(hinumaIndex, 1),
                               vis->hinuma_vectors(hinumaIndex, 2)});
  // this just takes the connection through the first two atoms as normal
  Eigen::Vector3f n = vis->positions[vis->activeEvent->frameNumber].row(vis->activeEvent->catalyst_atom_numbers[0])
      - vis->positions[vis->activeEvent->frameNumber].row(vis->activeEvent->chemical_atom_numbers[0]);
  vis->activeEvent->connectionNormal = glm::normalize(glm::vec3{n(0), n(1), n(2)});

  sqlite3_finalize(query);

}

void VisDataManager::addEventTags(const Event &event) {
  for (int atom_number : event.chemical_atom_numbers) {
    vis->tags(atom_number) |= Tags::eHighlighted;
  }
  for (int atom_number : event.catalyst_atom_numbers) {
    vis->tags(atom_number) |= Tags::eHighlighted;
  }
}

void VisDataManager::removeEventTags(const Event &event) {
  for (int atom_number : event.chemical_atom_numbers) {
    vis->tags(atom_number) &= (~Tags::eHighlighted);
  }
  for (int atom_number : event.catalyst_atom_numbers) {
    vis->tags(atom_number) &= (~Tags::eHighlighted);
  }
}

void VisDataManager::unloadActiveEvent() {
  vis->activeEvent.reset(nullptr);
}

Eigen::Vector<uint32_t, Eigen::Dynamic> &VisDataManager::getTagsRef() {
  return vis->tags;
}

void VisDataManager::removeSelectedByAreaTags() {
  for (uint32_t &tag : vis->tags) {
    tag &= (~Tags::eSelectedByArea);
  }
}

void VisDataManager::makeSelectedAreaChemical() {
  for (uint32_t &tag : vis->tags) {
    if ((tag & Tags::eSelectedByArea) == Tags::eSelectedByArea) {
      tag |= Tags::eChemical;
      tag &= (~Tags::eCatalyst);
    }
  }
}

void VisDataManager::makeSelectedAreaCatalyst() {
  for (uint32_t &tag : vis->tags) {
    if ((tag & Tags::eSelectedByArea) == Tags::eSelectedByArea) {
      tag |= Tags::eCatalyst;
      tag &= (~Tags::eChemical);
    }
  }
}


int VisDataManager::getChemicalBaseTypeID() {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db, "SELECT id FROM base_types WHERE name=\"chemical\"", -1, &query, nullptr);
  sqlite3_step(query);
  int baseTypeID = sqlite3_column_int(query, 0);
  sqlite3_finalize(query);
  return baseTypeID;
}

int VisDataManager::getCatalystBaseTypeID() {
  sqlite3_stmt *query;
  sqlite3_prepare_v2(db, "SELECT id FROM base_types WHERE name=\"catalyst\"", -1, &query, nullptr);
  sqlite3_step(query);
  int baseTypeID = sqlite3_column_int(query, 0);
  sqlite3_finalize(query);
  return baseTypeID;
}

int VisDataManager::getBaseTypePropertyID() const{
    sqlite3_stmt *query;
    sqlite3_prepare_v2(db, "SELECT id FROM properties WHERE name=\"init_base_type\"", -1, &query, nullptr);
    sqlite3_step(query);
    int baseTypePropertyID = sqlite3_column_int(query, 0);
    sqlite3_finalize(query);
    return baseTypePropertyID;
}

void VisDataManager::updatePropertyForSelectedAtomsToDB(int experimentID, int propertyID, int value) {

  sqlite3_stmt *transaction;
  sqlite3_prepare_v2(db, "BEGIN TRANSACTION", -1, &transaction, nullptr);
  sqlite3_step(transaction);

  sqlite3_stmt *query;
  sqlite3_prepare_v2(db, "UPDATE atom_tags"
                         " SET value = ? WHERE experiment_id = ? AND property_id = ?"
                         " AND atom_id = ?", -1, &query, nullptr);


  for (int i = 0; i < vis->atomIDs.size(); i++) {
    if(vis->tags[i] & Tags::eSelectedByArea){
      sqlite3_bind_int(query, 1, value);
      sqlite3_bind_int(query, 2, experimentID);
      sqlite3_bind_int(query, 3, propertyID);
      sqlite3_bind_int(query, 4, static_cast<int>(vis->atomIDs[i]));
      sqlite3_step(query);
      sqlite3_reset(query);
    }
  }

  sqlite3_prepare_v2(db, "COMMIT TRANSACTION", -1, &transaction, nullptr);

  sqlite3_finalize(query);
  sqlite3_finalize(transaction);
}

void VisDataManager::negateSelectedByAreaTags() {
  for (uint32_t &tag : vis->tags) {
    tag ^= Tags::eSelectedByArea;
  }
}

} // namespace rcc