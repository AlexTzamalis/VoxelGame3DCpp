#include "renderer/PlayerRenderer.hpp"
#include "renderer/Camera.hpp"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include "renderer/TextureAtlas.hpp"

PlayerRenderer::PlayerRenderer() : vao_(0), vbo_(0), textureID_(0), vertexCount_(0) {
}

PlayerRenderer::~PlayerRenderer() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    // Do NOT delete textureID_, it is owned by TextureAtlas
}

void PlayerRenderer::addCube(glm::vec3 center, glm::vec3 size, int u, int v, int w, int h, int d) {
    float iw = 1.0f / 64.0f;
    float ih = 1.0f / 64.0f;
    
    auto getUV = [iw, ih](float x, float y) {
        return glm::vec2(x * iw, y * ih);
    };

    glm::vec3 hs = size * 0.5f;

    glm::vec3 p0(-hs.x, -hs.y,  hs.z); // +Z, -X
    glm::vec3 p1( hs.x, -hs.y,  hs.z); // +Z, +X
    glm::vec3 p2( hs.x,  hs.y,  hs.z); // +Z, +X
    glm::vec3 p3(-hs.x,  hs.y,  hs.z); // +Z, -X
    glm::vec3 p4(-hs.x, -hs.y, -hs.z); // -Z, -X
    glm::vec3 p5( hs.x, -hs.y, -hs.z); // -Z, +X
    glm::vec3 p6( hs.x,  hs.y, -hs.z); // -Z, +X
    glm::vec3 p7(-hs.x,  hs.y, -hs.z); // -Z, -X

    auto addFace = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 n, glm::vec2 uv0, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3) {
        vertices_.push_back({v0 + center, uv0, n});
        vertices_.push_back({v1 + center, uv1, n});
        vertices_.push_back({v2 + center, uv2, n});
        vertices_.push_back({v0 + center, uv0, n});
        vertices_.push_back({v2 + center, uv2, n});
        vertices_.push_back({v3 + center, uv3, n});
    };

    auto makeFace = [&](glm::vec3 pBL, glm::vec3 pBR, glm::vec3 pTR, glm::vec3 pTL, glm::vec3 n, int tx, int ty, int tw, int th) {
        glm::vec2 uvBL = getUV(tx, ty + th);
        glm::vec2 uvBR = getUV(tx + tw, ty + th);
        glm::vec2 uvTR = getUV(tx + tw, ty);
        glm::vec2 uvTL = getUV(tx, ty);
        addFace(pBL, pBR, pTR, pTL, n, uvBL, uvBR, uvTR, uvTL);
    };

    // Front (-Z)
    makeFace(p5, p4, p7, p6, glm::vec3(0, 0, -1), u + d, v + d, w, h);
    // Back (+Z)
    makeFace(p0, p1, p2, p3, glm::vec3(0, 0, 1), u + d + w + d, v + d, w, h);
    // Right (+X)
    makeFace(p1, p5, p6, p2, glm::vec3(1, 0, 0), u, v + d, d, h);
    // Left (-X)
    makeFace(p4, p0, p3, p7, glm::vec3(-1, 0, 0), u + d + w, v + d, d, h);
    // Top (+Y)
    makeFace(p6, p7, p3, p2, glm::vec3(0, 1, 0), u + d, v, w, d);
    // Bottom (-Y)
    makeFace(p1, p0, p4, p5, glm::vec3(0, -1, 0), u + d + w, v, w, d);
}

void PlayerRenderer::buildMesh() {
    vertices_.clear();

    // 1 pixel = ~ 0.05625 blocks
    float px = 0.05625f;

    // Head (8x8x8)
    // Pivot at base of neck. 
    // Size is 8x8x8 px. Pivot is at y=0. Middle is at y=4px.
    addCube(glm::vec3(0.0f, 4.0f * px, 0.0f), glm::vec3(8.0f * px, 8.0f * px, 8.0f * px), 0, 0, 8, 8, 8); // 0-35

    // Body (8x12x4)
    // Pivot at bottom. Middle is at y=6px.
    addCube(glm::vec3(0.0f, 6.0f * px, 0.0f), glm::vec3(8.0f * px, 12.0f * px, 4.0f * px), 16, 16, 8, 12, 4); // 36-71

    // Right Arm (4x12x4) (+X)
    addCube(glm::vec3(0.0f, -6.0f * px, 0.0f), glm::vec3(4.0f * px, 12.0f * px, 4.0f * px), 40, 16, 4, 12, 4); // 72-107

    // Left Arm (4x12x4) (-X)
    addCube(glm::vec3(0.0f, -6.0f * px, 0.0f), glm::vec3(4.0f * px, 12.0f * px, 4.0f * px), 32, 48, 4, 12, 4); // 108-143

    // Right Leg (4x12x4) (+X)
    addCube(glm::vec3(0.0f, -6.0f * px, 0.0f), glm::vec3(4.0f * px, 12.0f * px, 4.0f * px), 0, 16, 4, 12, 4); // 144-179

    // Left Leg (4x12x4) (-X)
    addCube(glm::vec3(0.0f, -6.0f * px, 0.0f), glm::vec3(4.0f * px, 12.0f * px, 4.0f * px), 16, 48, 4, 12, 4); // 180-215

    vertexCount_ = vertices_.size();

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), vertices_.data(), GL_STATIC_DRAW);
}

void PlayerRenderer::init() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Grab the skin from the TextureAtlas instead of manually parsing the zip
    textureID_ = TextureAtlas::getGuiTexture("default-skin-atlas");
    if (!textureID_) {
        std::cerr << "Failed to load player skin 'default-skin-atlas.png' from texture pack.\n";
    }

    buildMesh();
    // Required: add a way to pass shader since we removed it from arguments,
    // actually we should modify the header to pass Shader& shader to render().
    // Wait, let's keep it simple: the caller binds the shader. We just set the model matrix uniform.
}

void PlayerRenderer::render(const Camera& camera, float time, glm::vec3 velocity, bool isSprinting, int shaderId) {

    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID_);

    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    
    static float lastTime = time;
    float dt = time - lastTime;
    if (dt < 0.0f) dt = 0.0f;
    lastTime = time;
    
    static float animationPhase = 0.0f;
    float swingFreq = std::min(speed * 1.5f, 15.0f); 
    if (speed > 0.1f) {
        animationPhase += swingFreq * dt;
    }

    float currentSwing = std::min(speed * 0.15f, 1.2f); 
    if (speed < 0.1f) currentSwing = 0.0f;
    
    static float smoothSwing = 0.0f;
    smoothSwing += (currentSwing - smoothSwing) * 15.0f * dt;

    float armSwing = sin(animationPhase) * smoothSwing;
    float legSwing = sin(animationPhase) * smoothSwing;

    // Body origin (at feet)
    glm::vec3 playerPos = camera.position();
    playerPos.y -= 1.62f; // Offset from eye to feet

    bool isFirstPerson = camera.getViewMode() == CameraViewMode::FIRST_PERSON;
    float px = 0.05625f;

    if (isFirstPerson) {
        // Push the rendered body slightly backwards in 1st person
        // so looking down reveals the chest!
        float pushOft = 3.0f * px;
        playerPos.x -= cos(glm::radians(camera.yaw())) * pushOft;
        playerPos.z -= sin(glm::radians(camera.yaw())) * pushOft;
    }

    float facingYaw = glm::radians(-camera.yaw() - 90.0f); // Minecraft standard
    
    // We pass the model to the shader via standard glUniformMatrix4fv
    int modelLoc = glGetUniformLocation(shaderId, "model");

    auto drawPart = [&](int offset, glm::vec3 localPos, glm::vec3 rotAxis, float rotAngle) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, playerPos);
        model = glm::rotate(model, facingYaw, glm::vec3(0, 1, 0));
        model = glm::translate(model, localPos);
        if (rotAngle != 0.0f) model = glm::rotate(model, rotAngle, rotAxis);
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glDrawArrays(GL_TRIANGLES, offset * 36, 36);
    };

    if (!isFirstPerson) {
        // 0: Head (pivot neck, rot pitch)
        drawPart(0, glm::vec3(0, 24 * px, 0), glm::vec3(1, 0, 0), glm::radians(-camera.pitch()));
        // 1: Body (pivot bottom)
        drawPart(1, glm::vec3(0, 12 * px, 0), glm::vec3(1, 0, 0), 0.0f);
    }
    
    // 2: Right Arm (pivot shoulder) (+X)
    drawPart(2, glm::vec3(6 * px, 24 * px, 0), glm::vec3(1, 0, 0), -armSwing);
    
    // 3: Left Arm (pivot shoulder) (-X)
    drawPart(3, glm::vec3(-6 * px, 24 * px, 0), glm::vec3(1, 0, 0), armSwing);
    
    // 4: Right Leg (pivot hip) (+X)
    drawPart(4, glm::vec3(2 * px, 12 * px, 0), glm::vec3(1, 0, 0), legSwing);
    
    // 5: Left Leg (pivot hip) (-X)
    drawPart(5, glm::vec3(-2 * px, 12 * px, 0), glm::vec3(1, 0, 0), -legSwing);
}
