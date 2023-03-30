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

struct Experiments {
  using id_tuple = std::tuple<int, int, int>;
  std::vector<id_tuple> experimentSystemSettingIDs;
};

struct SqlPositionReaderHelper {
  int indexCounter = 0;
  int atomCount = 0;
  std::vector<Eigen::MatrixX3f> *allPositions = nullptr;
};

class VisDataLoader {
 public:
  VisDataLoader(const std::string &db_filepath, int experiment_id);
  //TODO: delete copy and assignment op's
  void exportExperiments(Experiments &experiments);
  void exportSettingText(int settingID, SettingsText &settings);
  void exportEvents(int experimentID, EventsText &events);

  void loadActiveEvent(int eventID);
  void unloadActiveEvent();

  const VisualizationData &data() { return *vis; }
  void load(int experiment_id);
  int getActiveExperiment() { return experimentID_; }
  int getActiveSystem() { return systemID_; }
  int getActiveSetting() { return settingID_; }

  Eigen::Vector<uint32_t, Eigen::Dynamic> &getTagsRef();
  void addEventTags(const Event &event);
  void removeEventTags(const Event &event);
  void removeSelectedByAreaTags();

 private:
  std::unique_ptr<VisualizationData> vis;
  void loadUnitCell(int systemID);
  void loadElementInfos(int systemID);
  void loadAtomPositions(int systemID);
  void loadHinuma(int experimentID);
  void loadAtomElementNumbersAndTags(int systemID);

  void loadBonds(int settingID);

  int experimentID_, systemID_, settingID_;
  sqlite3 *db = nullptr;
};

} // namespace rcc