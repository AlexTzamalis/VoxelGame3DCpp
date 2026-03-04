#include "Camera.hpp"
#include <algorithm>

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : position_(position), yaw_(yaw), pitch_(pitch), worldUp_(0.0f, 1.0f, 0.0f) {
    updateVectors();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(fov_), aspect_, near_, far_);
}

void Camera::move(float dx, float dy, float dz) {
    position_.x += dx;
    position_.y += dy;
    position_.z += dz;
}

void Camera::rotate(float deltaYaw, float deltaPitch) {
    yaw_ += deltaYaw;
    pitch_ += deltaPitch;
    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
    updateVectors();
}

void Camera::updateVectors() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    dir.y = sin(glm::radians(pitch_));
    dir.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(dir);
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    up_ = glm::normalize(glm::cross(right_, front_));
}
