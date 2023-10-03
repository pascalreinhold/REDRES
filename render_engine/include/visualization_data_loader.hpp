//
// Created by x on 3/1/23.
//
#pragma once

#include "visualization_data.hpp"
#include <memory>
#include <sqlite3.h>
#include <iostream>
#include <vector>

namespace rcc {

struct SettingsText {
  using row = std::array<std::string, 3>; //name value description
  std::vector<row> parameters;
};

struct EventsText {
  using event_tuple = std::tuple<int, int, std::string, std::string>; // frame, event_type, description
  std::vector<event_tuple> events;
};

struct ExperimentIDTriplets {
  using id_tuple = std::tuple<int, int, int>;
  std::vector<id_tuple> experimentSystemSettingIDs;
};

struct SqlPositionReaderHelper {
  int indexCounter = 0;
  int atomCount = 0;
  std::vector<Eigen::MatrixX3f> *allPositions = nullptr;
};

class VisDataManager {

 public:

  VisDataManager(const std::string &db_filepath);
  ~VisDataManager();
  //TODO: delete copy and assignment op's
  void updatePropertyForSelectedAtomsToDB(int experimentID, int propertyID, int value);
  int getCatalystBaseTypeID();
  int getChemicalBaseTypeID();
  void exportExperimentIDTriplets(ExperimentIDTriplets &experiments);
  void exportSettingText(int settingID, SettingsText &settings);
  void exportEvents(int experimentID, EventsText &events);

  void loadActiveEvent(int eventID);
  void unloadActiveEvent();
  void load(int experiment_id);
  void unload();


  [[nodiscard]] const VisualizationData &data() const { return *vis; }
  [[nodiscard]] int getActiveExperiment() const { return experimentID_; }
  [[nodiscard]] int getActiveSystem() const { return systemID_; }
  [[nodiscard]] int getActiveSetting() const { return settingID_; }
  [[nodiscard]] const std::string &getDBFilepath() const { return db_filepath_; }
  [[nodiscard]] int getBaseTypePropertyID() const;
  [[nodiscard]] int getExperimentCount();
  [[nodiscard]] int getFirstExperimentID();

  Eigen::Vector<uint32_t, Eigen::Dynamic> &getTagsRef();
  void addEventTags(const Event &event);
  void negateSelectedByAreaTags();
  void removeEventTags(const Event &event);
  void removeSelectedByAreaTags();
  void removeSelectedForMeasurementTags();
  void makeSelectedAreaChemical();
  void makeSelectedAreaCatalyst();

 private:
  void loadUnitCell(int systemID);
  void loadElementInfos(int systemID);
  void loadAtomPositions(int systemID);
  void loadHinuma(int experimentID);
  void loadAtomElementNumbersAndTags(int experimentID);

  void connectToDB(int open_db_flags = SQLITE_OPEN_READONLY);
  void disconnectFromDB();

  void loadBonds(int settingID);

  std::unique_ptr<VisualizationData> vis;
  int experimentID_ = -1, systemID_ = -1, settingID_ = -1;

  sqlite3 *db = nullptr;
  std::string db_filepath_;
};

} // namespace rcc