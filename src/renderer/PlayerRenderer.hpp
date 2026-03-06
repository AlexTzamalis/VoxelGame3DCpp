#pragma once

#include <glm/glm.hpp>
#include <vector>

class Camera; // Forward declaration

class PlayerRenderer {
public:
    PlayerRenderer();
    ~PlayerRenderer();

    void init();
    void render(const Camera& camera, float time, glm::vec3 velocity, bool isSprinting, int shaderId);

private:
    unsigned int vao_, vbo_;
    unsigned int textureID_;
    int vertexCount_;

    // To construct the mesh
    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec3 normal;
    };
    std::vector<Vertex> vertices_;

    void addCube(glm::vec3 center, glm::vec3 size, int u, int v, int w, int h, int d);
    void buildMesh();
};
