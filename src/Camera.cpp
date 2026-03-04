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

void Camera::jump(float velocity) {
    if (isGrounded_) {
        physicsVelocity_.y = velocity;
        isGrounded_ = false;
    }
}

void Camera::applyPhysics(float deltaTime, const std::function<bool(glm::vec3)>& checkCollisionFunc) {
    // 1. Apply Gravity to Physics Velocity
    physicsVelocity_.y -= 28.0f * deltaTime; // Gravity acceleration

    // 2. Combine intentional player input with gravity
    glm::vec3 targetVelocity = inputVelocity_ + physicsVelocity_;
    glm::vec3 moveDelta = targetVelocity * deltaTime;

    // We calculate "Player Base" position since position_ is our eye level
    glm::vec3 playerBase = position_;
    playerBase.y -= eyeOffsetY; 

    // 3. Collision testing per axis (to allow sliding across walls)
    // Create an AABB checking function
    auto collides = [&](glm::vec3 pos) -> bool {
        // Sample standard 8 corners of the bounding box
        glm::vec3 minB = pos - boundingBoxHalfExtents_;
        glm::vec3 maxB = pos + boundingBoxHalfExtents_;

        for (float x = minB.x; x <= maxB.x; x += boundingBoxHalfExtents_.x * 2.0f) {
            for (float y = minB.y; y <= maxB.y; y += boundingBoxHalfExtents_.y) {
                for (float z = minB.z; z <= maxB.z; z += boundingBoxHalfExtents_.z * 2.0f) {
                    if (checkCollisionFunc(glm::vec3(x, y, z))) return true;
                }
            }
        }
        return false;
    };

    // X-Axis Test
    playerBase.x += moveDelta.x;
    if (collides(playerBase)) {
        playerBase.x -= moveDelta.x; // Revert
        inputVelocity_.x = 0;
    }

    // Z-Axis Test
    playerBase.z += moveDelta.z;
    if (collides(playerBase)) {
        playerBase.z -= moveDelta.z; // Revert
        inputVelocity_.z = 0;
    }

    // Y-Axis Test
    playerBase.y += moveDelta.y;
    isGrounded_ = false;
    if (collides(playerBase)) {
        playerBase.y -= moveDelta.y; // Revert
        if (moveDelta.y < 0) { // Hit the floor (falling down)
            isGrounded_ = true;
        }
        physicsVelocity_.y = 0;
    }

    // 4. Finalize position
    position_ = playerBase;
    position_.y += eyeOffsetY; // Return to eye-level offset

    // Decay purely intentional input movement instantly (like traditional WASD handling)
    inputVelocity_ = glm::vec3(0.0f);
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

void Camera::updateFrustum() {
    glm::mat4 viewProj = projectionMatrix() * viewMatrix();

    // Left
    frustum_.planes[0].normal.x = viewProj[0][3] + viewProj[0][0];
    frustum_.planes[0].normal.y = viewProj[1][3] + viewProj[1][0];
    frustum_.planes[0].normal.z = viewProj[2][3] + viewProj[2][0];
    frustum_.planes[0].distance = viewProj[3][3] + viewProj[3][0];

    // Right
    frustum_.planes[1].normal.x = viewProj[0][3] - viewProj[0][0];
    frustum_.planes[1].normal.y = viewProj[1][3] - viewProj[1][0];
    frustum_.planes[1].normal.z = viewProj[2][3] - viewProj[2][0];
    frustum_.planes[1].distance = viewProj[3][3] - viewProj[3][0];

    // Bottom
    frustum_.planes[2].normal.x = viewProj[0][3] + viewProj[0][1];
    frustum_.planes[2].normal.y = viewProj[1][3] + viewProj[1][1];
    frustum_.planes[2].normal.z = viewProj[2][3] + viewProj[2][1];
    frustum_.planes[2].distance = viewProj[3][3] + viewProj[3][1];

    // Top
    frustum_.planes[3].normal.x = viewProj[0][3] - viewProj[0][1];
    frustum_.planes[3].normal.y = viewProj[1][3] - viewProj[1][1];
    frustum_.planes[3].normal.z = viewProj[2][3] - viewProj[2][1];
    frustum_.planes[3].distance = viewProj[3][3] - viewProj[3][1];

    // Near
    frustum_.planes[4].normal.x = viewProj[0][3] + viewProj[0][2];
    frustum_.planes[4].normal.y = viewProj[1][3] + viewProj[1][2];
    frustum_.planes[4].normal.z = viewProj[2][3] + viewProj[2][2];
    frustum_.planes[4].distance = viewProj[3][3] + viewProj[3][2];

    // Far
    frustum_.planes[5].normal.x = viewProj[0][3] - viewProj[0][2];
    frustum_.planes[5].normal.y = viewProj[1][3] - viewProj[1][2];
    frustum_.planes[5].normal.z = viewProj[2][3] - viewProj[2][2];
    frustum_.planes[5].distance = viewProj[3][3] - viewProj[3][2];

    for (int i = 0; i < 6; i++) {
        float length = glm::length(frustum_.planes[i].normal);
        frustum_.planes[i].normal /= length;
        frustum_.planes[i].distance /= length;
    }
}

bool Camera::isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const {
    for (int i = 0; i < 6; i++) {
        glm::vec3 p = min;
        if (frustum_.planes[i].normal.x >= 0) p.x = max.x;
        if (frustum_.planes[i].normal.y >= 0) p.y = max.y;
        if (frustum_.planes[i].normal.z >= 0) p.z = max.z;

        if (glm::dot(frustum_.planes[i].normal, p) + frustum_.planes[i].distance < 0) {
            return false;
        }
    }
    return true;
}
