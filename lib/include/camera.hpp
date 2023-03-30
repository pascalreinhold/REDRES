//
// Created by x on 11/22/22.
//
#include "utils.hpp"

#include <glm/vec3.hpp>
#include <map>
#include <GLFW/glfw3.h>

namespace rcc {

class Camera {
 public:

  struct PerspectiveViewSettings {
    float near, far, perspective_fovy, move_speed, turn_speed;
  } perspective_view_settings_;

  struct IsometricViewSettings {
    float isometric_height, isometric_depth, zoom_speed;
  } isometric_view_settings_;

  explicit Camera(const PerspectiveViewSettings &pSettings, const IsometricViewSettings &iSettings);
  void UpdateCamera(float frame_time, GLFWwindow *glfwWindow);
  [[nodiscard]] glm::mat4 GetProjectionMatrix(const vk::Extent2D &window_extent) const;
  [[nodiscard]] glm::mat4 GetViewMatrix() const;
  [[nodiscard]] glm::vec3 GetViewDirection() const { return view_direction_; }
  [[nodiscard]] glm::vec3 GetPosition() const { return position_; }
  [[nodiscard]] glm::vec3 GetUp() const { return up_direction_; }

  glm::vec3 system_center{0.f};
  glm::vec2 isometric_offset_ = {0.f, 0.f};
  glm::vec3 position_ = {0.f, 0.f, 1.f};
  glm::vec3 view_direction_ = {0.f, -1.f, 0.f};
  glm::vec3 up_direction_ = {0.f, 0.f, 1.f};
  float drag_speed_ = 1.f;
  bool is_isometric;

  enum keyBindings {
    moveLeft = GLFW_KEY_A,
    moveRight = GLFW_KEY_D,
    moveForward = GLFW_KEY_W,
    moveBackward = GLFW_KEY_S,

    moveUp = GLFW_KEY_E,
    moveDown = GLFW_KEY_Q,

    lookLeft = GLFW_KEY_LEFT,
    lookRight = GLFW_KEY_RIGHT,
    lookUp = GLFW_KEY_UP,
    lookDown = GLFW_KEY_DOWN,

    rotateClockwise = GLFW_KEY_R,
    rotateCounterClockwise = GLFW_KEY_F,

    sneak = GLFW_KEY_LEFT_SHIFT,
    sprint = GLFW_KEY_LEFT_CONTROL
  };
};

}