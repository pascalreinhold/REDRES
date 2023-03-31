//
// Created by x on 3/4/23.
//

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ImGuiFileDialog.h"
#include "gui.hpp"
#include <glm/gtx/string_cast.hpp>

namespace {
void vkCheck(VkResult err) {
  if (err==0)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}

ImVec4 gammaCorrect(int r, int g, int b, int a) {
  float gamma = 2.2f;
  return ImVec4{
      powf(static_cast<float>(r)/255.f, gamma),
      powf(static_cast<float>(g)/255.f, gamma),
      powf(static_cast<float>(b)/255.f, gamma),
      static_cast<float>(a)/255.f};
}
}

namespace rcc {

UserInterface::UserInterface(rcc::Engine *parent) : parentEngine{parent} {
  bLightMode = Engine::getConfig()["UseLightMode"].get<bool>();
  initImGui();
}

void UserInterface::showMainMenubar() {
  bool clicked = false;
  if (ImGui::BeginMainMenuBar()) {
    titleBarHeight = ImGui::GetWindowHeight();
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Load Database")) {
        clicked = true;
      }
      if (ImGui::MenuItem("Unload Database")) {
        parentEngine->scene_->visLoader.reset(nullptr);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("User Preferences")) {
        preferencesWindowVisible = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      for (const auto type : parentEngine->scene_->objectTypes) {
        if (parentEngine->scene_->visLoader) {
          if (type->isLoaded()) {
            std::string label = "Show " + type->typeIdentifier;
            ImGui::Checkbox(label.c_str(), &type->shown);
          }
        }
      }

      ImGui::Separator();

      ImGui::SliderFloat("Atom Size", &parentEngine->scene_->gConfig.atomSize, 0, 4);
      ImGui::SliderFloat("Bond Length", &parentEngine->scene_->gConfig.bondLength, 0, 4);
      ImGui::SliderFloat("Bond Thickness", &parentEngine->scene_->gConfig.bondThickness, 0, 4);
      ImGui::SliderFloat("Hinuma Vector Length", &parentEngine->scene_->gConfig.hinumaVectorLength, 0, 2);
      ImGui::SliderFloat("Hinuma Vector Thickness", &parentEngine->scene_->gConfig.hinumaVectorThickness, 0, 4);

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug Windows")) {
      ImGui::Checkbox("Show Debug Window", &debugWindowVisible);
      ImGui::Checkbox("Show Material Parameter Window", &materialParameterWindowVisible);

#ifdef RCC_GUI_DEV_MODE
      ImGui::Separator();
      ImGui::Checkbox("Show Demo Window", &demoWindowVisible);
      ImGui::Checkbox("Show Style Test Window", &styleTestWindowVisible);
      ImGui::Checkbox("Show StackTool Window", &stackToolVisible);
#endif
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
  showFileDialog(clicked);
}

void UserInterface::showEventsTable(int experimentID) {
  if (ImGui::BeginTable("EventsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    // load setting from database if not already loaded
    if (loadedEventsText.find(experimentID)==loadedEventsText.end()) {
      EventsText eventsText;
      parentEngine->scene_->visLoader->exportEvents(experimentID, eventsText);
      loadedEventsText.insert({experimentID, eventsText});
    }

    ImGui::TableSetupColumn("EventID");
    ImGui::TableSetupColumn("FrameID");
    ImGui::TableSetupColumn("Event Type");
    ImGui::TableHeadersRow();

    const auto &events = loadedEventsText[experimentID].events;
    for (int j = 0; j < events.size(); j++) {

      bool isActive = false;
      if (parentEngine->scene_->visLoader->data().activeEvent) isActive = std::get<0>(events[j])
            ==parentEngine->scene_->visLoader->data().activeEvent->eventID;

      ImGui::TableNextColumn();
      ImGui::Text(isActive ? "%d, active" : "%d", std::get<0>(events[j]));
      ImGui::TableNextColumn();
      ImGui::Text("%d", std::get<1>(events[j]));
      ImGui::TableNextColumn();
      ImGui::Text("%s", std::get<2>(events[j]).c_str());

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*35.0f);
        ImGui::TextUnformatted(std::get<3>(events[j]).c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
      }
      ImGui::TableNextColumn();
      ImGui::PushID(j);
      if (ImGui::SmallButton("jump")) {
        parentEngine->enterEventMode(std::get<0>(events[j]));
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

void UserInterface::showEventInfoWindow() {
  float width = static_cast<float>(parentEngine->window_->width())*debugWindowRelativeWidth;
  float height = (static_cast<float>(parentEngine->window_->height()) - titleBarHeight - secondaryTitleBarHeight)
      *debugWindowRelativeHeight;

  ImGui::SetNextWindowSize({width, height});
  ImGui::SetNextWindowPos({static_cast<float>(parentEngine->window_->width()) - width + 1.f,
                           titleBarHeight + secondaryTitleBarHeight + height});

  if (ImGui::Begin("Event Info", nullptr, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
    if (ImGui::BeginChild("Event Info Child")) {
      ImGui::Checkbox("Cylinder Culling", &parentEngine->scene_->eventViewerSettings.enableCylinderCulling);

      // toggled checkboxes
      static bool useConnectionNormal = false;
      parentEngine->scene_->eventViewerSettings.surfaceNormals = !useConnectionNormal;
      ImGui::Checkbox("Use Surface Normal", &parentEngine->scene_->eventViewerSettings.surfaceNormals);
      useConnectionNormal = !parentEngine->scene_->eventViewerSettings.surfaceNormals;
      ImGui::Checkbox("Use Connection Normal", &useConnectionNormal);

      if (ImGui::Button("Leave Event Mode")) {
        parentEngine->leaveEventMode();
      } else {
        ImGui::SliderFloat("CylinderLength", &parentEngine->scene_->eventViewerSettings.cylinderLength, 0, 100);
        ImGui::SliderFloat("CylinderRadius", &parentEngine->scene_->eventViewerSettings.cylinderRadius, 0, 30);
        for (const auto &atomIndex : parentEngine->scene_->visLoader->data().activeEvent->chemical_atom_numbers) {
          ImGui::Text("Atom Number: %d", atomIndex);
        }
        for (const auto &atomIndex : parentEngine->scene_->visLoader->data().activeEvent->catalyst_atom_numbers) {
          ImGui::Text("Atom Number: %d", atomIndex);
        }
        ImGui::Text("Cylinder Center:");
        ImGui::Text("%s", glm::to_string(parentEngine->scene_->visLoader->data().activeEvent->center).c_str());
        ImGui::Text("Surface Normal:");
        ImGui::Text("%s", glm::to_string(parentEngine->scene_->visLoader->data().activeEvent->surfaceNormal).c_str());
        ImGui::Text("Connection Normal:");
        ImGui::Text("%s",
                    glm::to_string(parentEngine->scene_->visLoader->data().activeEvent->connectionNormal).c_str());
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

void UserInterface::showSettingTable(int settingID) {
  if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    // load setting from database if not already loaded
    if (loadedSettings.find(settingID)==loadedSettings.end()) {
      SettingsText setting;
      parentEngine->scene_->visLoader->exportSettingText(settingID, setting);
      loadedSettings.insert({settingID, setting});
    }

    ImGui::TableSetupColumn("Parameter");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    const auto &setting = loadedSettings[settingID];
    for (const auto &parameter : setting.parameters) {
      ImGui::TableNextColumn();
      ImGui::Text("%s", parameter[0].c_str());
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*35.0f);
        ImGui::TextUnformatted(parameter[2].c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
      }
      ImGui::TableNextColumn();
      ImGui::Text("%s", parameter[1].c_str());
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*35.0f);
        ImGui::TextUnformatted(parameter[2].c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
      }
    }
    ImGui::EndTable();
  }
}

void UserInterface::showLeftAlignedWindow() {
  {
    float width = static_cast<float>(parentEngine->window_->width())*leftAlignedWidgetRelativeSize;
    float height = static_cast<float>(parentEngine->window_->height()) - titleBarHeight - secondaryTitleBarHeight;

    ImGui::SetNextWindowSize({width, height});
    ImGui::SetNextWindowPos({0., titleBarHeight + secondaryTitleBarHeight});

    if (ImGui::Begin("Left Aligned", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysUseWindowPadding)) {

      if (ImGui::BeginChild("Experiments", {0.f, height*0.55f}, false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        if (experimentsNeedRefresh) {
          parentEngine->scene_->visLoader->exportExperiments(experiments_);
          experimentsNeedRefresh = false;
        }
        ImGui::Text("Experiments:");
        for (int i = 0; i < experiments_.experimentSystemSettingIDs.size(); i++) {
          ImGui::PushID(i);

          int experimentID = std::get<0>(experiments_.experimentSystemSettingIDs[i]);
          bool isActiveExperiment = experimentID==parentEngine->scene_->visLoader->getActiveExperiment();

          if (!isActiveExperiment) ImGui::AlignTextToFramePadding();
          bool experimentExpanded = ImGui::TreeNode("Experiment",
                                                    isActiveExperiment ? "Experiment  (ID=%d, active)"
                                                                       : "Experiment  (ID=%d)",
                                                    experimentID);

          if (!isActiveExperiment) {
            ImGui::SameLine();
            if (ImGui::SmallButton("load")) {
              parentEngine->scene_->visLoader->load(experimentID);
            }
          }

          if (experimentExpanded) {
            int systemID = std::get<1>(experiments_.experimentSystemSettingIDs[i]);
            if (ImGui::TreeNode("System", "System (ID=%d)", systemID)) {
              ImGui::TreePop();
            }

            int settingID = std::get<2>(experiments_.experimentSystemSettingIDs[i]);
            if (ImGui::TreeNode("Setting", "Setting (ID=%d)", settingID)) {
              showSettingTable(settingID);
              ImGui::TreePop();
            }

            if (ImGui::TreeNode("Events")) {
              showEventsTable(experimentID);
              ImGui::TreePop();
            }

            ImGui::TreePop();
          }
          ImGui::PopID();
        }
      }
      ImGui::EndChild();

      ImGui::Separator();

      Camera &cam = *parentEngine->camera_;
      auto &camPSettings = cam.perspective_view_settings_;
      auto &camISettings = cam.isometric_view_settings_;

      if (ImGui::BeginChild("Infos:", {0.f, 0.f}, false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        ImGui::InputFloat3("Camera Coords", reinterpret_cast<float *>(&parentEngine->camera_->position_));
        ImGui::InputFloat3("View Direction", reinterpret_cast<float *>(&parentEngine->camera_->view_direction_));
        ImGui::InputFloat3("Up Direction", reinterpret_cast<float *>(&parentEngine->camera_->up_direction_));

#ifdef RCC_GUI_DEV_MODE
        ImGui::InputFloat("Movement Speed", &camPSettings.move_speed);
        ImGui::InputFloat("Turn Speed", &camPSettings.turn_speed);
        ImGui::InputFloat("Zoom Speed", &camISettings.zoom_speed);
#endif

        if (!parentEngine->camera_->is_isometric) {
          ImGui::SliderFloat("Field of View",
                             &camPSettings.perspective_fovy,
                             30,
                             120,
                             "%.0f",
                             ImGuiSliderFlags_AlwaysClamp);
        }
      }
      ImGui::EndChild();
    }
    ImGui::End();
  }
}

void UserInterface::showDebugWindow() {
  float width = static_cast<float>(parentEngine->window_->width())*debugWindowRelativeWidth;
  float height = (static_cast<float>(parentEngine->window_->height()) - titleBarHeight - secondaryTitleBarHeight)
      *debugWindowRelativeHeight;

  ImGui::SetNextWindowSize({width, height});
  ImGui::SetNextWindowPos({static_cast<float>(parentEngine->window_->width()) - width + 1.f,
                           titleBarHeight + secondaryTitleBarHeight});

  if (ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
    if (ImGui::BeginChild("Debug Window Child")) {

      ImGui::Text("FPS: %f", 1000.0/parentEngine->framerate_control_.avgFrameTime.avg());

#ifdef RCC_GUI_DEV_MODE
      ImGui::SliderInt("FPS Cap:", &parentEngine->framerate_control_.max_framerate_, 1, 1000);
#endif
#ifdef RCC_GUI_DEV_MODE
      ImGui::Checkbox("Enable Culling", &parentEngine->isCullingEnabled);
#endif

      if (parentEngine->scene_->visLoader) {
        ImGui::SliderInt("Movie Framerate:", &parentEngine->framerate_control_.movie_framerate_, 1, 300);
        ImGui::SliderFloat("MovieFrameIndex",
                           &parentEngine->framerate_control_.movie_frame_index_,
                           0,
                           static_cast<float>(parentEngine->scene_->MovieFrameCount() - 1),
                           "%.0f",
                           ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Loop Simulation", &parentEngine->framerate_control_.isSimulationLooped);
        ImGui::Checkbox("Manual Movie Frame Control", &parentEngine->framerate_control_.manualFrameControl);

        if (ImGui::Button("Remove Selected by Area Tag")) {
          parentEngine->scene_->visLoader->removeSelectedByAreaTags();
        }

#ifdef RCC_GUI_DEV_MODE
        {
          auto dummy = ImGui::GetMousePos();
          ImGui::InputFloat2("Mouse Coords (ImGui:)", reinterpret_cast<float *>(&dummy[0]));

          double dummy2[2];
          glfwGetCursorPos(parentEngine->window_->glfwWindow_, &dummy2[0], &dummy2[1]);
          float dummy3[2] = {static_cast<float>(dummy2[0]), static_cast<float>(dummy2[1])};
          ImGui::InputFloat2("Mouse Coords (GLFW:)", dummy3);

          int dummy4[2] = {parentEngine->window_->width(), parentEngine->window_->height()};
          ImGui::InputInt2("Window Sizes", dummy4);
        }
#endif

        ImGui::Text("Selected Object Index: %d", parentEngine->selected_object_index_);
        ImGui::Text("Freeze Object Index: %d", parentEngine->scene_->freezeAtom());

#ifdef RCC_GUI_DEV_MODE
        for(const auto& type : parentEngine->scene_->objectTypes) {
          if(!(type->isLoaded() && type->shown)) continue;
          std::string text = "Number of " + type->typeIdentifier + "s: %u";
          ImGui::Text(text.c_str(), type->Count(parentEngine->framerate_control_.movie_frame_index_));
        }
#endif

        if (parentEngine->selected_object_index_!=-1) {
          ImGui::Text("%s",
                      parentEngine->scene_->getObjectInfo(static_cast<uint32_t>(parentEngine->framerate_control_.movie_frame_index_),
                                                          parentEngine->selected_object_index_).c_str());
          if (parentEngine->selected_object_index_
              < (*parentEngine->scene_)["Atom"].Count(static_cast<uint32_t>(parentEngine->framerate_control_.movie_frame_index_))) {
            int freezeID = parentEngine->scene_->freezeAtom();
            int selectID = parentEngine->selected_object_index_;

            static bool is_frozen;
            is_frozen = freezeID==selectID;
            bool aux = is_frozen;

            ImGui::Checkbox("Pick Freeze Atom", &is_frozen);
            if (is_frozen) { parentEngine->scene_->pickFreezeAtom(parentEngine->selected_object_index_); }
            if (!is_frozen && aux) { parentEngine->scene_->pickFreezeAtom(-1); }
          }
        }

        auto &cellX = parentEngine->scene_->gConfig.xCellCount;
        auto &cellY = parentEngine->scene_->gConfig.yCellCount;
        auto &cellZ = parentEngine->scene_->gConfig.zCellCount;
        auto &maxCells = parentEngine->max_cell_count_;

        ImGui::InputInt("Cells X", &cellX, 1, 2);
        ImGui::InputInt("Cells Y", &cellY, 1, 2);
        ImGui::InputInt("Cells Z", &cellZ, 1, 2);
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

void UserInterface::showMaterialParameterWindow() {
  ImGui::Begin("Material Parameter Window");
  for (int i = 0; i < RCC_MESH_COUNT; i++) {
    auto &reciprocal_gamma = parentEngine->scene_data_.params[i][0];
    auto &shininess = parentEngine->scene_data_.params[i][1];
    auto &diffuse_coeff = parentEngine->scene_data_.params[i][2];
    auto &specular_coeff = parentEngine->scene_data_.params[i][3];
    std::string text = parentEngine->scene_->objectTypes[i]->typeIdentifier + ": ";
    ImGui::Text("%s", text.c_str());
    ImGui::PushID(i);
    ImGui::SliderFloat("Reciprocal Gamma:##%d", &reciprocal_gamma, 0, 4);
    ImGui::SliderFloat("Shininess:", &shininess, 0, 2048, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Diffuse Coefficient:", &diffuse_coeff, 0, 5, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Specular Coefficient:", &specular_coeff, 0, 5, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::Separator();
    ImGui::PopID();
  }

  ImGui::End();
}

void UserInterface::showSecondaryMenubar() {

  secondaryTitleBarHeight = titleBarHeight*1.5f;
  auto width = static_cast<float>(parentEngine->window_->width());

  ImGui::SetNextWindowSize({width, secondaryTitleBarHeight});
  ImGui::SetNextWindowPos({0., titleBarHeight});

  ImGui::Begin("Secondary Menubar",
               nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
  ImGui::Text("Align View");
  ImGui::SameLine();

  float plus_button_size = ImGui::GetFrameHeight();
  if (ImGui::Button("X", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->view_direction_ = glm::vec3{1.f, 0.f, 0.f};
  }
  ImGui::SameLine();
  if (ImGui::Button("Y", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->view_direction_ = glm::vec3{0.f, 1.f, 0.f};
  }
  ImGui::SameLine();
  if (ImGui::Button("Z", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->view_direction_ = glm::vec3{0.f, 0.f, 1.f};
  }

  ImGui::SameLine();
  ImGui::Text("Align Up");
  ImGui::SameLine();
  if (ImGui::Button("X##up", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->up_direction_ = glm::vec3{1.f, 0.f, 0.f};
  }
  ImGui::SameLine();
  if (ImGui::Button("Y##up", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->up_direction_ = glm::vec3{0.f, 1.f, 0.f};
  }
  ImGui::SameLine();
  if (ImGui::Button("Z##up", ImVec2(plus_button_size, plus_button_size))) {
    parentEngine->camera_->up_direction_ = glm::vec3{0.f, 0.f, 1.f};
  }

  static float step_size = 45.f;
  ImGui::SameLine();
  ImGui::Text("Step Size(°):");
  ImGui::SameLine();
  ImGui::PushItemWidth(6.f*ImGui::GetFontSize());
  ImGui::InputFloat("##stepsize_input_float", &step_size, 15.f, 45.f, "%.1f");
  ImGui::PopItemWidth();

  glm::vec3 &view_dir = parentEngine->camera_->view_direction_;
  glm::vec3 &up_dir = parentEngine->camera_->up_direction_;
  glm::vec3 right_dir = glm::cross(view_dir, up_dir);

  ImGui::SameLine();
  ImGui::Text("yaw:");
  ImGui::SameLine();
  if (ImGui::Button("-##yawminus", ImVec2(plus_button_size, plus_button_size))) {
    view_dir = glm::mat3(glm::rotate(-glm::radians(step_size), up_dir))*view_dir;
  }
  ImGui::SameLine();
  if (ImGui::Button("+##yawplus", ImVec2(plus_button_size, plus_button_size))) {
    view_dir = glm::mat3(glm::rotate(glm::radians(step_size), up_dir))*view_dir;
  }

  ImGui::SameLine();
  ImGui::Text("roll:");
  ImGui::SameLine();
  if (ImGui::Button("-##rollminus", ImVec2(plus_button_size, plus_button_size))) {
    up_dir = glm::mat3(glm::rotate(-glm::radians(step_size), view_dir))*up_dir;
  }
  ImGui::SameLine();
  if (ImGui::Button("+##rollplus", ImVec2(plus_button_size, plus_button_size))) {
    up_dir = glm::mat3(glm::rotate(glm::radians(step_size), view_dir))*up_dir;
  }

  ImGui::SameLine();
  ImGui::Text("pitch:");
  ImGui::SameLine();
  if (ImGui::Button("-##pitchminus", ImVec2(plus_button_size, plus_button_size))) {
    up_dir = glm::mat3(glm::rotate(-glm::radians(step_size), right_dir))*up_dir;
    view_dir = glm::mat3(glm::rotate(-glm::radians(step_size), right_dir))*view_dir;
  }
  ImGui::SameLine();
  if (ImGui::Button("+##pitchplus", ImVec2(plus_button_size, plus_button_size))) {
    up_dir = glm::mat3(glm::rotate(glm::radians(step_size), right_dir))*up_dir;
    view_dir = glm::mat3(glm::rotate(glm::radians(step_size), right_dir))*view_dir;
  }

  ImGui::End();
}

void UserInterface::showPreferencesWindow() {
  float width = static_cast<float>(parentEngine->window_->width())*0.4f;
  float height = static_cast<float>(parentEngine->window_->height())*0.8f;

  Camera &cam = *parentEngine->camera_;
  auto &camPSettings = cam.perspective_view_settings_;
  auto &camISettings = cam.isometric_view_settings_;

  ImGui::SetNextWindowSize({width, height});
  if (ImGui::Begin("User Preferences",
                   &preferencesWindowVisible,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {

    ImGui::Text("Color Theme");
    const char *text = (bLightMode) ? "Toggle to dark mode" : "Toggle to light mode";
    if (ImGui::Button(text)) {
      if (bLightMode) {
        setupGuiStyleDark();
      } else {
        setupGuiStyle();
      }
      bLightMode = !bLightMode;
    }
    ImGui::ColorEdit3("Background Color", &parentEngine->clearColor[0]);
    ImGui::Separator();

    ImGui::Text("Camera and Movement");
    ImGui::InputFloat("Movement Speed", &camPSettings.move_speed);
    ImGui::InputFloat("Turn Speed", &camPSettings.turn_speed);
    ImGui::InputFloat("Drag Speed", &parentEngine->camera_->drag_speed_);
    ImGui::InputFloat("Zoom Speed", &camISettings.zoom_speed);
    ImGui::InputFloat("Near Plane", &camPSettings.near);
    ImGui::InputFloat("Far Plane", &camPSettings.far);
    ImGui::InputFloat("Isometric Depth", &camISettings.isometric_depth);
    if (!parentEngine->camera_->is_isometric) {
      ImGui::SliderFloat("Field of View",
                         &camPSettings.perspective_fovy,
                         30,
                         120,
                         "%.0f",
                         ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::Separator();

    ImGui::Text("Rendering Parameters");
    ImGui::SliderFloat("Atom Size", &parentEngine->scene_->gConfig.atomSize, 0, 4);
    ImGui::SliderFloat("Bond Length", &parentEngine->scene_->gConfig.bondLength, 0, 4);
    ImGui::SliderFloat("Bond Thickness", &parentEngine->scene_->gConfig.bondThickness, 0, 4);
    ImGui::SliderFloat("Hinuma Vector Length", &parentEngine->scene_->gConfig.hinumaVectorLength, 0, 2);
    ImGui::SliderFloat("Hinuma Vector Thickness", &parentEngine->scene_->gConfig.hinumaVectorThickness, 0, 4);
    ImGui::Separator();

    ImGui::Text("Further Options");
  }
  ImGui::End();
}

void UserInterface::show() {

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (mainMenubarVisible) showMainMenubar();
  if (parentEngine->scene_->visLoader) showSecondaryMenubar();
  if (parentEngine->scene_->visLoader) showLeftAlignedWindow();

  if (parentEngine->scene_->visLoader) {
    if (parentEngine->scene_->visLoader->data().activeEvent!=nullptr) showEventInfoWindow();
  }

  if (materialParameterWindowVisible) showMaterialParameterWindow();

  if (styleTestWindowVisible) {
    ImGui::Begin("Style Test");
    ImGui::ShowStyleEditor();    // add style editor block (not a window). you can pass in a reference ImGuiStyle structure to compare to, revert to and save to (else it uses the default style)
    ImGui::End();
  }
  if (debugWindowVisible) showDebugWindow();
  if (stackToolVisible) ImGui::ShowStackToolWindow();
  if (demoWindowVisible) ImGui::ShowDemoWindow();
  if (preferencesWindowVisible) showPreferencesWindow();

  // draw selection rectangle
  if (!wantMouse() && parentEngine->camera_->is_isometric) {
    static ImVec2 start{0.0, 0.0};
    static ImVec2 end{0.0, 0.0};

    if (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl) {
      start = {-1.0, -1.0};
    }

    if (showSelectionRectangle(start, end)) {
      // do something with the selection
      parentEngine->selectAtomsWithRect(
          glm::vec2{start[0], start[1]},
          glm::vec2{end[0], end[1]},
          parentEngine->GetMovieFrameIndex());
    }
  }

}

bool UserInterface::showSelectionRectangle(ImVec2 &start_pos, ImVec2 &end_pos) {

  // set start position if clicked
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    start_pos = ImGui::GetMousePos();
    return false;
  }

  // if start position is set, draw rectangle
  if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if ((start_pos.x < 0 || start_pos.y < 0)) {
      return false;
    } else {
      end_pos = ImGui::GetMousePos();
      ImDrawList *draw_list = ImGui::GetForegroundDrawList();
      ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_SliderGrab];
      // transform color to 0-255 range
      color.x *= 255;
      color.y *= 255;
      color.z *= 255;

      draw_list->AddRect(start_pos, end_pos, ImGui::GetColorU32(IM_COL32(color.x, color.y, color.z, 255)));
      draw_list->AddRectFilled(start_pos, end_pos, ImGui::GetColorU32(IM_COL32(color.x, color.y, color.z, 50)));

    }
  }

  // if mouse is released and start position was set return bMouseReleased
  if ((start_pos.x < 0 || start_pos.y < 0)) {
    return false;
  } else {
    return ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  }
}

void UserInterface::setupGuiStyle() {
  //Light Mode standard style
  ImGuiStyle &style = ImGui::GetStyle();

  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.6000000238418579f;
  style.WindowPadding = ImVec2(8.0f, 8.0f);
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowMinSize = ImVec2(32.0f, 32.0f);
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_Right;
  style.ChildRounding = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupRounding = 8.0f;
  style.PopupBorderSize = 0.0f;
  style.FramePadding = ImVec2(4.0f, 3.0f);
  style.FrameRounding = 8.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(12.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 4.0f);
  style.CellPadding = ImVec2(4.0f, 2.0f);
  style.IndentSpacing = 20.0f;
  style.ColumnsMinSpacing = 6.0f;
  style.ScrollbarSize = 11.0f;
  style.ScrollbarRounding = 2.0f;
  style.GrabMinSize = 10.0f;
  style.GrabRounding = 2.0f;
  style.TabRounding = 8.0f;
  style.TabBorderSize = 0.0f;
  style.TabMinWidthForCloseButton = 0.0f;
  style.ColorButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
  style.Colors[ImGuiCol_Text] = gammaCorrect(0, 0, 0, 255);
  style.Colors[ImGuiCol_TextDisabled] = gammaCorrect(100, 100, 100, 255);
  style.Colors[ImGuiCol_WindowBg] = gammaCorrect(242, 242, 242, 255);
  style.Colors[ImGuiCol_ChildBg] = gammaCorrect(105, 105, 105, 0);
  style.Colors[ImGuiCol_PopupBg] = gammaCorrect(224, 224, 224, 255);
  style.Colors[ImGuiCol_Border] = gammaCorrect(155, 155, 155, 255);
  style.Colors[ImGuiCol_BorderShadow] = gammaCorrect(0, 0, 0, 0);
  style.Colors[ImGuiCol_FrameBg] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_FrameBgHovered] = gammaCorrect(140, 140, 140, 102);
  style.Colors[ImGuiCol_FrameBgActive] = gammaCorrect(130, 130, 130, 176);
  style.Colors[ImGuiCol_TitleBg] = gammaCorrect(140, 140, 140, 255);
  style.Colors[ImGuiCol_TitleBgActive] = gammaCorrect(140, 140, 140, 255);
  style.Colors[ImGuiCol_TitleBgCollapsed] = gammaCorrect(140, 140, 140, 100);
  style.Colors[ImGuiCol_MenuBarBg] = gammaCorrect(224, 224, 224, 204);
  style.Colors[ImGuiCol_ScrollbarBg] = gammaCorrect(224, 224, 224, 204);
  style.Colors[ImGuiCol_ScrollbarGrab] = gammaCorrect(81, 81, 81, 97);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = gammaCorrect(97, 97, 97, 102);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = gammaCorrect(52, 52, 52, 153);
  style.Colors[ImGuiCol_CheckMark] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_SliderGrab] = gammaCorrect(35, 132, 255, 140);
  style.Colors[ImGuiCol_SliderGrabActive] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_Button] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_ButtonHovered] = gammaCorrect(35, 132, 255, 120);
  style.Colors[ImGuiCol_ButtonActive] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_Header] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_HeaderHovered] = gammaCorrect(35, 132, 255, 120);
  style.Colors[ImGuiCol_HeaderActive] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_Separator] = gammaCorrect(170, 170, 170, 255);
  style.Colors[ImGuiCol_SeparatorHovered] = gammaCorrect(170, 170, 170, 255);
  style.Colors[ImGuiCol_SeparatorActive] = gammaCorrect(170, 170, 170, 255);
  style.Colors[ImGuiCol_ResizeGrip] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_ResizeGripHovered] = gammaCorrect(35, 132, 255, 120);
  style.Colors[ImGuiCol_ResizeGripActive] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_Tab] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_TabHovered] = gammaCorrect(35, 132, 255, 120);
  style.Colors[ImGuiCol_TabActive] = gammaCorrect(35, 132, 255, 255);
  style.Colors[ImGuiCol_TabUnfocused] = gammaCorrect(35, 132, 255, 60);
  style.Colors[ImGuiCol_TabUnfocusedActive] = gammaCorrect(35, 132, 255, 125);
  style.Colors[ImGuiCol_PlotLines] = gammaCorrect(255, 255, 255, 255);
  style.Colors[ImGuiCol_PlotLinesHovered] = gammaCorrect(230, 179, 0, 255);
  style.Colors[ImGuiCol_PlotHistogram] = gammaCorrect(230, 179, 0, 255);
  style.Colors[ImGuiCol_PlotHistogramHovered] = gammaCorrect(255, 153, 0, 255);
  style.Colors[ImGuiCol_TableHeaderBg] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_TableBorderStrong] = gammaCorrect(0, 0, 0, 255);
  style.Colors[ImGuiCol_TableBorderLight] = gammaCorrect(0, 0, 0, 50);
  style.Colors[ImGuiCol_TableRowBg] = gammaCorrect(0, 0, 0, 15);
  style.Colors[ImGuiCol_TableRowBgAlt] = gammaCorrect(255, 255, 255, 18);
  style.Colors[ImGuiCol_TextSelectedBg] = gammaCorrect(35, 132, 255, 99);
  style.Colors[ImGuiCol_DragDropTarget] = gammaCorrect(255, 255, 0, 230);
  style.Colors[ImGuiCol_NavHighlight] = gammaCorrect(115, 115, 230, 240);
  style.Colors[ImGuiCol_NavWindowingHighlight] = gammaCorrect(255, 255, 255, 179);
  style.Colors[ImGuiCol_NavWindowingDimBg] = gammaCorrect(204, 204, 204, 51);
  style.Colors[ImGuiCol_ModalWindowDimBg] = gammaCorrect(51, 51, 51, 89);
}

void UserInterface::setupGuiStyleDark() {
  //Light Mode standard style
  ImGuiStyle &style = ImGui::GetStyle();

  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.6000000238418579f;
  style.WindowPadding = ImVec2(8.0f, 8.0f);
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowMinSize = ImVec2(32.0f, 32.0f);
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_Right;
  style.ChildRounding = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupRounding = 8.0f;
  style.PopupBorderSize = 0.0f;
  style.FramePadding = ImVec2(4.0f, 3.0f);
  style.FrameRounding = 8.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(12.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 4.0f);
  style.CellPadding = ImVec2(4.0f, 2.0f);
  style.IndentSpacing = 20.0f;
  style.ColumnsMinSpacing = 6.0f;
  style.ScrollbarSize = 11.0f;
  style.ScrollbarRounding = 2.0f;
  style.GrabMinSize = 10.0f;
  style.GrabRounding = 2.0f;
  style.TabRounding = 8.0f;
  style.TabBorderSize = 0.0f;
  style.TabMinWidthForCloseButton = 0.0f;
  style.ColorButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
  style.Colors[ImGuiCol_Text] = gammaCorrect(255, 255, 255, 255);
  style.Colors[ImGuiCol_TextDisabled] = gammaCorrect(180, 180, 180, 255);
  style.Colors[ImGuiCol_WindowBg] = gammaCorrect(60, 60, 63, 255);
  style.Colors[ImGuiCol_ChildBg] = gammaCorrect(105, 105, 105, 0);
  style.Colors[ImGuiCol_PopupBg] = gammaCorrect(45, 45, 48, 255);
  style.Colors[ImGuiCol_Border] = gammaCorrect(155, 155, 155, 255);
  style.Colors[ImGuiCol_BorderShadow] = gammaCorrect(0, 0, 0, 0);
  style.Colors[ImGuiCol_FrameBg] = gammaCorrect(120, 120, 120, 99);
  style.Colors[ImGuiCol_FrameBgHovered] = gammaCorrect(140, 140, 140, 102);
  style.Colors[ImGuiCol_FrameBgActive] = gammaCorrect(120, 120, 120, 176);
  style.Colors[ImGuiCol_TitleBg] = gammaCorrect(45, 45, 48, 255);
  style.Colors[ImGuiCol_TitleBgActive] = gammaCorrect(45, 45, 48, 255);
  style.Colors[ImGuiCol_TitleBgCollapsed] = gammaCorrect(45, 45, 48, 120);
  style.Colors[ImGuiCol_MenuBarBg] = gammaCorrect(40, 40, 43, 204);
  style.Colors[ImGuiCol_ScrollbarBg] = gammaCorrect(45, 45, 48, 150);
  style.Colors[ImGuiCol_ScrollbarGrab] = gammaCorrect(120, 120, 120, 97);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = gammaCorrect(130, 130, 130, 102);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = gammaCorrect(160, 160, 160, 153);
  style.Colors[ImGuiCol_CheckMark] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_SliderGrab] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_SliderGrabActive] = gammaCorrect(247, 215, 14, 255);
  style.Colors[ImGuiCol_Button] = gammaCorrect(120, 120, 120, 99);
  style.Colors[ImGuiCol_ButtonHovered] = gammaCorrect(255, 185, 20, 180);
  style.Colors[ImGuiCol_ButtonActive] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_Header] = gammaCorrect(140, 140, 140, 99);
  style.Colors[ImGuiCol_HeaderHovered] = gammaCorrect(255, 185, 20, 180);
  style.Colors[ImGuiCol_HeaderActive] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_Separator] = gammaCorrect(170, 170, 170, 80);
  style.Colors[ImGuiCol_SeparatorHovered] = gammaCorrect(170, 170, 170, 80);
  style.Colors[ImGuiCol_SeparatorActive] = gammaCorrect(170, 170, 170, 80);
  style.Colors[ImGuiCol_ResizeGrip] = gammaCorrect(170, 170, 170, 99);
  style.Colors[ImGuiCol_ResizeGripHovered] = gammaCorrect(255, 185, 20, 180);
  style.Colors[ImGuiCol_ResizeGripActive] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_Tab] = gammaCorrect(120, 120, 120, 99);
  style.Colors[ImGuiCol_TabHovered] = gammaCorrect(255, 185, 20, 180);
  style.Colors[ImGuiCol_TabActive] = gammaCorrect(255, 185, 20, 255);
  style.Colors[ImGuiCol_TabUnfocused] = gammaCorrect(255, 185, 20, 90);
  style.Colors[ImGuiCol_TabUnfocusedActive] = gammaCorrect(255, 185, 20, 125);
  style.Colors[ImGuiCol_PlotLines] = gammaCorrect(255, 255, 255, 255);
  style.Colors[ImGuiCol_PlotLinesHovered] = gammaCorrect(230, 179, 0, 255);
  style.Colors[ImGuiCol_PlotHistogram] = gammaCorrect(230, 179, 0, 255);
  style.Colors[ImGuiCol_PlotHistogramHovered] = gammaCorrect(255, 153, 0, 255);
  style.Colors[ImGuiCol_TableHeaderBg] = gammaCorrect(140, 140, 140, 99);
  style.Colors[ImGuiCol_TableBorderStrong] = gammaCorrect(0, 0, 0, 0);
  style.Colors[ImGuiCol_TableBorderLight] = gammaCorrect(0, 0, 0, 50);
  style.Colors[ImGuiCol_TableRowBg] = gammaCorrect(0, 0, 0, 15);
  style.Colors[ImGuiCol_TableRowBgAlt] = gammaCorrect(255, 255, 255, 18);
  style.Colors[ImGuiCol_TextSelectedBg] = gammaCorrect(35, 132, 255, 99);
  style.Colors[ImGuiCol_DragDropTarget] = gammaCorrect(255, 255, 0, 230);
  style.Colors[ImGuiCol_NavHighlight] = gammaCorrect(115, 115, 230, 240);
  style.Colors[ImGuiCol_NavWindowingHighlight] = gammaCorrect(255, 255, 255, 179);
  style.Colors[ImGuiCol_NavWindowingDimBg] = gammaCorrect(204, 204, 204, 51);
  style.Colors[ImGuiCol_ModalWindowDimBg] = gammaCorrect(51, 51, 51, 89);
}

void UserInterface::initImGui() {

  vk::DescriptorPoolSize pool_sizes[] =
      {
          {vk::DescriptorType::eSampler, 500},
          {vk::DescriptorType::eCombinedImageSampler, 500},
          {vk::DescriptorType::eSampledImage, 500},
          {vk::DescriptorType::eStorageImage, 500},
          {vk::DescriptorType::eUniformBuffer, 500},
          {vk::DescriptorType::eStorageTexelBuffer, 500},
          {vk::DescriptorType::eUniformBuffer, 500},
          {vk::DescriptorType::eStorageBuffer, 500},
          {vk::DescriptorType::eUniformBufferDynamic, 500},
          {vk::DescriptorType::eStorageBufferDynamic, 500},
          {vk::DescriptorType::eInputAttachment, 500}
      };

  vk::DescriptorPoolCreateInfo pool_info = vk::DescriptorPoolCreateInfo(
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      1000, std::size(pool_sizes), pool_sizes);
  VkDescriptorPool imguiPool = parentEngine->logical_device_.createDescriptorPool(pool_info);

  ImGui::CreateContext();

  const std::string imgui_ini_path = Engine::getConfig()["AssetDirectoryFilepath"].get<std::string>()
                                   + Engine::getConfig()["ImGuiIniFilepath"].get<std::string>();
  ImGui::GetIO().IniFilename = imgui_ini_path.c_str();

  if (bLightMode) {
    setupGuiStyle();
  } else {
    setupGuiStyleDark();
  }


  ImGui_ImplGlfw_InitForVulkan(parentEngine->window_->glfwWindow_, true);
  ImGui_ImplVulkan_InitInfo init_info = {};

  init_info.Instance = parentEngine->instance_;
  init_info.PhysicalDevice = parentEngine->physical_device_;
  init_info.Device = parentEngine->logical_device_;
  //init_info.QueueFamily = graphics_queue_family_;
  init_info.Queue = parentEngine->graphics_queue_;
  //init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = imguiPool;
  //init_info.Subpass = 0;
  init_info.MinImageCount = parentEngine->swapchain_->imageCount();
  init_info.ImageCount = parentEngine->swapchain_->imageCount();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  //init_info.Allocator = g_Allocator;
  init_info.CheckVkResultFn = vkCheck;

  ImGui_ImplVulkan_Init(&init_info, parentEngine->swapchain_->renderPass());

  ImGuiIO &io = ImGui::GetIO();
  std::string font_filepath = Engine::getConfig()["AssetDirectoryFilepath"].get<std::string>()
      + Engine::getConfig()["FontFilepath"].get<std::string>();
  io.Fonts->AddFontFromFileTTF(font_filepath.c_str(), 18);

  //io.Fonts->GetTexDataAsRGBA32();

  parentEngine->immediateSubmit([&](vk::CommandBuffer cmd) {
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
  });

  ImGui_ImplVulkan_DestroyFontUploadObjects();

  parentEngine->main_destruction_stack_.push([=]() {
#ifdef RCC_DESTROY_MESSAGES
    std::cout << "Destroying ImGui DescriptorPool \n";
#endif
    parentEngine->logical_device_.destroy(imguiPool);
    ImGui_ImplVulkan_Shutdown();
  });
}

bool UserInterface::wantMouse() {
  return ImGui::GetIO().WantCaptureMouse;
}

bool UserInterface::wantKeyboard() {
  return ImGui::GetIO().WantCaptureKeyboard;
}

void UserInterface::writeDrawDataToCmdBuffer(vk::CommandBuffer &cmd) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UserInterface::render() { ImGui::Render(); }

void UserInterface::showFileDialog(bool clicked) {
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(parentEngine->window_->width())*0.6f,
                                  static_cast<float>(parentEngine->window_->width())*0.6f*0.6f), ImGuiCond_Once);
  // open Dialog Simple
  if (clicked)
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose Database File", ".db", ".");

  // display
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    // action if OK
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      parentEngine->scene_->visLoader.reset(nullptr);
      parentEngine->scene_->visLoader = std::make_unique<VisDataLoader>(filePathName, 1);
    }
    // close
    ImGuiFileDialog::Instance()->Close();
  }
}

} // namespace rcc