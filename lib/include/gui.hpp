//
// Created by x on 3/4/23.
//

#include "engine.hpp"
#include "imgui.h"

#pragma once
namespace rcc {

class UserInterface {
 public:
  explicit UserInterface(Engine *parent);

  void show();
  static bool wantMouse();
  static bool wantKeyboard();
  static void writeDrawDataToCmdBuffer(vk::CommandBuffer &cmd);
  static void render();

 private:
  void initImGui();

  static void setupGuiStyle();
  static void setupGuiStyleDark();

  Engine *parentEngine;

  // light mode
  bool bLightMode = true;

  // sizes
  float titleBarHeight = 10.f; //not the actual value. just to not divide by zero in the first frames
  float secondaryTitleBarHeight = 15.f;
  float leftAlignedWidgetRelativeSize = 0.22f;
  float debugWindowRelativeHeight = 0.5f;
  float debugWindowRelativeWidth = 0.22;

  // visibility
  bool mainMenubarVisible = true;
  bool styleTestWindowVisible = false;
  bool materialParameterWindowVisible = false;
  bool debugWindowVisible = true;
  bool stackToolVisible = false;
  bool demoWindowVisible = false;

  // experiments
  bool experimentsNeedRefresh = true;
  bool preferencesWindowVisible = false;
  Experiments experiments_;

  // cached database queries (ordered by id's):
  std::map<int, SettingsText> loadedSettings;
  std::map<int, EventsText> loadedEventsText;

  // windows + menubar
  void showLeftAlignedWindow();
  void showMainMenubar();
  void showSecondaryMenubar();
  void showEventInfoWindow();
  void showFileDialog(bool clicked);
  void showDebugWindow();
  void showMaterialParameterWindow();
  void showPreferencesWindow();

  // widgets
  void showSettingTable(int settingID);
  void showEventsTable(int experimentID);
  static bool showSelectionRectangle(ImVec2 &start_pos, ImVec2 &end_pos);

  friend class Engine;
};

} // namespace rcc