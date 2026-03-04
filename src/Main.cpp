#include "Camera.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "ChunkManager.hpp"
#include "Config.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>

namespace {
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float lastX = Config::windowWidth / 2.0f;
    float lastY = Config::windowHeight / 2.0f;
    bool firstMouse = true;

    Camera camera(glm::vec3(0.0f, 15.0f, 5.0f));
    ChunkManager* globalChunkManager = nullptr;

    bool raycast(glm::vec3 start, glm::vec3 direction, float maxDistance, glm::ivec3& hitPos, glm::ivec3& prevPos) {
        glm::vec3 current = start;
        glm::vec3 step = direction * 0.05f; 
        
        glm::ivec3 lastIntPos(std::floor(current.x), std::floor(current.y), std::floor(current.z));
        float distance = 0.0f;

        while (distance < maxDistance) {
            current += step;
            distance += 0.05f;
            
            glm::ivec3 intPos(std::floor(current.x), std::floor(current.y), std::floor(current.z));
            
            if (intPos != lastIntPos) {
                uint8_t voxel = globalChunkManager->getVoxelGlobal(intPos.x, intPos.y, intPos.z);
                if (voxel > 1 && voxel != 5) { // Solid blocks only (ignore transparent Air=1 and Water=5)
                    hitPos = intPos;
                    prevPos = lastIntPos;
                    return true;
                }
                lastIntPos = intPos;
            }
        }
        return false;
    }

    void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        if (action == GLFW_PRESS && globalChunkManager != nullptr) {
            glm::ivec3 hitPos, prevPos;
            if (raycast(camera.position(), camera.front(), 8.0f, hitPos, prevPos)) {
                if (button == GLFW_MOUSE_BUTTON_LEFT) {
                    globalChunkManager->setVoxelGlobal(hitPos.x, hitPos.y, hitPos.z, 1); // 1 = Air
                } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                    globalChunkManager->setVoxelGlobal(prevPos.x, prevPos.y, prevPos.z, 4); // 4 = Stone
                }
            }
        }
    }

    void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
        Config::windowWidth = width;
        Config::windowHeight = height;
        camera.setAspect(static_cast<float>(width) / static_cast<float>(height));
    }

    void processInput(GLFWwindow* window) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        float speed = Config::playerSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) speed = Config::playerSprintSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.addPosition(camera.front() * speed);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.addPosition(-camera.front() * speed);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.addPosition(-camera.right() * speed);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.addPosition(camera.right() * speed);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.addPosition(glm::vec3(0.0f, 1.0f, 0.0f) * speed);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera.addPosition(glm::vec3(0.0f, -1.0f, 0.0f) * speed);
    }

    void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (firstMouse) {
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
            firstMouse = false;
        }
        float dx = static_cast<float>(xpos) - lastX;
        float dy = lastY - static_cast<float>(ypos);
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        camera.rotate(dx * Config::mouseSensitivity, dy * Config::mouseSensitivity);
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, "Voxel Game 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to init GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    Shader shader("shaders/basic.vert", "shaders/basic.frag");
    if (shader.id() == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    Texture atlas;
    if (!atlas.loadFromFile("assets/sprite-atlas-1.png")) {
        std::cerr << "Texture atlas not found. Run from build directory.\n";
    }

    ChunkManager chunkManager;
    globalChunkManager = &chunkManager;

    camera.setAspect(static_cast<float>(Config::windowWidth) / static_cast<float>(Config::windowHeight));
    camera.setFov(Config::cameraFov);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        chunkManager.update(camera.position());

        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        
        shader.setMat4("view", camera.viewMatrix());
        shader.setMat4("projection", camera.projectionMatrix());
        shader.setVec3("lightDir", glm::normalize(glm::vec3(0.5f, -1.0f, 0.3f)));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader.setInt("textureAtlas", 0);

        atlas.bind(0);
        
        // Update the Frustum boundary definitions based on currently moving camera coordinates
        camera.updateFrustum();

        // Pass the actual ID of the shader, and the camera so it can perform visibility testing bounds!
        chunkManager.render(shader.id(), camera);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
