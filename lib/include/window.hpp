//
// Created by x on 1/10/23.
//
#pragma once

#include <string>

//Forward Declarations
class GLFWwindow;

namespace vk {
class Instance;
class SurfaceKHR;
}

namespace rcc {
class Window {
 public:
  Window(int width, int height, std::string name);
  ~Window();

  // Delete Copy and Assignment Operations
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  [[nodiscard]] int height() const;
  [[nodiscard]] int width() const;
  [[nodiscard]] double aspect() const;

  [[nodiscard]] bool wasResized() const { return wasWindowResized; }
  static void windowResizeCallback(GLFWwindow *window, int width, int height);
  void resetWasResizedFlag() { wasWindowResized = false; }

  [[nodiscard]] bool shouldClose() const;
  void createSurface(class vk::Instance &instance, class vk::SurfaceKHR *surface) const;
  GLFWwindow *glfwWindow_ = nullptr;

 private:
  void initWindow();
  int width_, height_;
  bool wasWindowResized = false;
  std::string windowName_;
};
}