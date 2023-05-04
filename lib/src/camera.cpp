//
// Created by x on 11/22/22.
//

#include "camera.hpp"
#include <glm/gtx/transform.hpp>

namespace rcc {

glm::mat4 Camera::GetViewMatrix() const {
    // transforms camera to origin (TO CAMERA SPACE)
    if (is_isometric) {
        return glm::lookAt(system_center - view_direction_, system_center, up_direction_);
    }
    return glm::lookAt(position_, position_ + view_direction_, up_direction_);
}

glm::mat4 Camera::GetProjectionMatrix(const vk::Extent2D &window_extent) const {
    // transforms frustum to normalized clipping coordinates (vulkan has z values in [0-1] instead of [-1, 1] in openGL)
    float aspectRatio = static_cast<float>(window_extent.width)/static_cast<float>(window_extent.height);
    glm::mat4 projection{};
    if (!is_isometric) {
        projection = glm::perspective(glm::radians(perspective_view_settings_.perspective_fovy),
                                      aspectRatio, perspective_view_settings_.near, perspective_view_settings_.far);
    } else {
        projection = glm::ortho(-isometric_view_settings_.isometric_height*aspectRatio + isometric_offset_[0],
                                isometric_view_settings_.isometric_height*aspectRatio + isometric_offset_[0],
                                -isometric_view_settings_.isometric_height + isometric_offset_[1],
                                isometric_view_settings_.isometric_height + isometric_offset_[1],
                                -isometric_view_settings_.isometric_depth,
                                isometric_view_settings_.isometric_depth);
    }
    projection[1][1] *= -1;
    return projection;
}

void Camera::UpdateCamera(float frame_time, GLFWwindow *glfwWindow) {

    if (glm::dot(up_direction_, up_direction_) > 1e-4) {
        up_direction_ = glm::normalize(up_direction_);
    }

    if (glm::dot(view_direction_, view_direction_) > 1e-4) {
        view_direction_ = glm::normalize(view_direction_);
    }

    static bool lastFrameIsometric = false;
    if (!lastFrameIsometric) isometric_offset_ = {0.f, 0.f};
    lastFrameIsometric = is_isometric;

    float dx = frame_time*perspective_view_settings_.move_speed;
    float up_axis_rotation = 0.f;
    float right_axis_rotation = 0.f;
    float view_axis_rotation = 0.f;
    glm::vec3 translation{0.f};
    glm::vec2 iso_translation{0.f, 0.f};

    const glm::vec3 right_direction{glm::normalize(glm::cross(view_direction_, up_direction_))};

    if (glfwGetKey(glfwWindow, keyBindings::moveForward)==GLFW_PRESS) {
        translation += view_direction_;
        iso_translation[1] -= 1;
    }

    if (glfwGetKey(glfwWindow, keyBindings::moveBackward)==GLFW_PRESS) {
        translation -= view_direction_;
        iso_translation[1] += 1;
    }

    if (glfwGetKey(glfwWindow, keyBindings::moveLeft)==GLFW_PRESS) {
        translation -= right_direction;
        iso_translation[0] -= 1;
    }

    if (glfwGetKey(glfwWindow, keyBindings::moveRight)==GLFW_PRESS) {
        translation += right_direction;
        iso_translation[0] += 1;
    }

    if (glfwGetKey(glfwWindow, keyBindings::moveUp)==GLFW_PRESS) {
        translation -= up_direction_;
    }

    if (glfwGetKey(glfwWindow, keyBindings::moveDown)==GLFW_PRESS) {
        translation += up_direction_;
    }

    if (glfwGetKey(glfwWindow, keyBindings::lookRight)==GLFW_PRESS) up_axis_rotation--;
    if (glfwGetKey(glfwWindow, keyBindings::lookLeft)==GLFW_PRESS) up_axis_rotation++;

    if (glfwGetKey(glfwWindow, keyBindings::lookUp)==GLFW_PRESS) right_axis_rotation++;
    if (glfwGetKey(glfwWindow, keyBindings::lookDown)==GLFW_PRESS) right_axis_rotation--;

    if (glfwGetKey(glfwWindow, keyBindings::rotateClockwise)==GLFW_PRESS) view_axis_rotation++;
    if (glfwGetKey(glfwWindow, keyBindings::rotateCounterClockwise)==GLFW_PRESS) view_axis_rotation--;

    float speed_amplifier = 1;
    if (glfwGetKey(glfwWindow, keyBindings::sneak)==GLFW_PRESS) speed_amplifier = 0.3f;
    if (glfwGetKey(glfwWindow, keyBindings::sprint)==GLFW_PRESS) speed_amplifier = 2.8f;


    // only translate if movement is non zero
    if (right_axis_rotation!=0) {
        view_direction_ = glm::normalize(
            glm::mat3(glm::rotate(right_axis_rotation*perspective_view_settings_.turn_speed*speed_amplifier*frame_time,
                                  right_direction))
                *view_direction_);
        up_direction_ = glm::normalize(
            glm::mat3(glm::rotate(right_axis_rotation*perspective_view_settings_.turn_speed*speed_amplifier*frame_time,
                                  right_direction))
                *up_direction_);
    }

    if (up_axis_rotation!=0) {
        view_direction_ = glm::normalize(
            glm::mat3(glm::rotate(up_axis_rotation*perspective_view_settings_.turn_speed*speed_amplifier*frame_time,
                                  up_direction_))
                *view_direction_);
    }

    if (view_axis_rotation!=0) {
        up_direction_ = glm::normalize(
            glm::mat3(glm::rotate(view_axis_rotation*perspective_view_settings_.turn_speed*speed_amplifier*frame_time,
                                  view_direction_))
                *up_direction_);
    }

    if (is_isometric) {
        isometric_offset_ += dx*speed_amplifier*iso_translation;
    } else {
        if (glm::dot(translation, translation) > std::numeric_limits<float>::epsilon()) {
            position_ += dx*speed_amplifier*glm::normalize(translation);
        }
    }

}

Camera::Camera(const PerspectiveViewSettings &pSettings, const IsometricViewSettings &iSettings) :
    perspective_view_settings_{pSettings},
    isometric_view_settings_{iSettings},
    is_isometric{false} {}

void Camera::alignPerspectivePositionToSystemCenter(float dist) {
    position_ = system_center - dist*view_direction_;
}

}  //namespace rcc