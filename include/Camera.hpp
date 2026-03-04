#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Plane {
    glm::vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];
};

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const { return position_; }
    glm::vec3 front() const { return front_; }
    glm::vec3 right() const { return right_; }
    glm::vec3 up() const { return up_; }

    void move(float dx, float dy, float dz);
    void addPosition(glm::vec3 delta) { position_ += delta; }
    void rotate(float deltaYaw, float deltaPitch);
    void setAspect(float aspect) { aspect_ = aspect; }
    void setFov(float fov) { fov_ = fov; }
    glm::mat4 projectionMatrix() const;

    void updateFrustum();
    bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const;

private:
    void updateVectors();

    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 worldUp_;

    float yaw_;
    float pitch_;
    float aspect_ = 16.0f / 9.0f;
    float fov_ = 60.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;

    Frustum frustum_;
};
