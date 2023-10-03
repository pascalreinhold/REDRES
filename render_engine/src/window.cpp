//
// Created by x on 1/10/23.
//

#include "window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>

#include "vulkan_include.hpp"

namespace rcc {

Window::Window(int width, int height, std::string  windowName) :
    width_{width},
    height_{height},
    windowName_{std::move(windowName)} {
  initWindow();
}

void Window::setWindowName(const std::string& new_window_name) {
  windowName_ = new_window_name;
  glfwSetWindowTitle(glfwWindow_, windowName_.c_str());
}

Window::~Window() {
  glfwDestroyWindow(glfwWindow_);
  glfwTerminate();
}

void Window::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); //TODO CHANGE THIS BACK TO TRUE
  glfwWindow_ = glfwCreateWindow(width_, height_, windowName_.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(glfwWindow_, this);
  glfwSetFramebufferSizeCallback(glfwWindow_, windowResizeCallback);
}

void Window::createSurface(class vk::Instance &instance, class vk::SurfaceKHR *surface) const {
  if (glfwCreateWindowSurface(instance, glfwWindow_, nullptr, reinterpret_cast<VkSurfaceKHR *>(surface))!=VK_SUCCESS) {
    throw std::runtime_error("Surface creation failed");
  }
}

int Window::height() const {
  return height_;
}

int Window::width() const {
  return width_;
}

double Window::aspect() const {
  return static_cast<double>(width_)/static_cast<double>(height_);
}

bool Window::shouldClose() const {
  return glfwWindowShouldClose(glfwWindow_);
}
void Window::windowResizeCallback(GLFWwindow *window, int width, int height) {
  auto w = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window)); // recover window owner pointer
  w->wasWindowResized = true;
  w->width_ = width;
  w->height_ = height;
}

}