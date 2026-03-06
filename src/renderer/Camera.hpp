#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>

struct Plane {
    glm::vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];
};

enum class CameraViewMode {
    FIRST_PERSON,
    THIRD_PERSON_BACK,
    THIRD_PERSON_FRONT
};

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const { return position_; }
    void setPosition(glm::vec3 pos) { position_ = pos; physicsVelocity_ = glm::vec3(0.0f); flyVelocity_ = glm::vec3(0.0f); }
    float pitch() const { return pitch_; }
    void setPitch(float p) { pitch_ = p; }
    float yaw() const { return yaw_; }
    void setYaw(float y) { yaw_ = y; }
    glm::vec3 front() const { return front_; }
    glm::vec3 right() const { return right_; }
    glm::vec3 up() const { return up_; }

    void jump(float velocity);
    void applyPhysics(float deltaTime, const std::function<bool(glm::vec3, glm::vec3)>& checkCollisionFunc);
    void updateVectors();
    
    // Movement input — sets desired direction, physics handles smoothing
    void addVelocity(glm::vec3 delta) { inputVelocity_ += delta; }
    void rotate(float deltaYaw, float deltaPitch);
    void setAspect(float aspect) { aspect_ = aspect; }
    void setFov(float fov) { fov_ = fov; }
    float getEffectiveFov() const;
    glm::mat4 projectionMatrix() const;
    
    // Get combined velocity for animation
    glm::vec3 getVelocity() const { return physicsVelocity_ + flyVelocity_; }

    // Zoom system
    void setZooming(bool zooming) { isZooming_ = zooming; }
    bool isZooming() const { return isZooming_; }
    void adjustZoom(float scrollDelta);
    float getZoomFov() const { return zoomTargetFov_; }

    // Sprint FOV boost
    void setSprinting(bool sprinting) { isSprinting_ = sprinting; }

    // Camera View Modes
    CameraViewMode getViewMode() const { return viewMode_; }
    void setViewMode(CameraViewMode mode) { viewMode_ = mode; }
    void toggleViewMode();
    glm::vec3 getRenderPosition() const; // The actual viewpoint, potentially offset for 3rd person

    void updateFrustum();
    bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const;

private:
    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 worldUp_;

    glm::vec3 inputVelocity_{0.0f};
    glm::vec3 physicsVelocity_{0.0f};
    glm::vec3 flyVelocity_{0.0f};       // Current smoothed flying velocity
    bool isGrounded_ = false;

    // Player dimensions: 0.6 wide, 1.8 tall, 0.6 deep
    // Eye is at 1.62 blocks from feet (Minecraft standard)
    const glm::vec3 boundingBoxHalfExtents_ = glm::vec3(0.3f, 0.9f, 0.3f);
    const float eyeOffsetY_ = 1.62f;

    float yaw_;
    float pitch_;
    float aspect_ = 16.0f / 9.0f;
    float fov_ = 60.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;

    // Zoom
    bool isZooming_ = false;
    float currentZoomFov_;
    float zoomTargetFov_ = 30.0f;

    // Sprint FOV
    bool isSprinting_ = false;
    float sprintFovOffset_ = 0.0f; // Current sprint FOV boost (smoothly interpolated)

    // View Mode
    CameraViewMode viewMode_ = CameraViewMode::FIRST_PERSON;
    float thirdPersonDistance_ = 4.0f;

    Frustum frustum_;
    
    // Helper function for checking ray collision (needs to be injected or we can just pass a simple raycast)
    std::function<float(glm::vec3, glm::vec3, float)> raycastFunc_;
public:
    void setRaycastFunc(std::function<float(glm::vec3, glm::vec3, float)> func) { raycastFunc_ = func; }
};
