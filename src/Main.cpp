#include "Camera.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Chunk.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>

namespace {
    const int kWindowWidth = 1280;
    const int kWindowHeight = 720;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float lastX = kWindowWidth / 2.0f;
    float lastY = kWindowHeight / 2.0f;
    bool firstMouse = true;

    Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));

    void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
        camera.setAspect(static_cast<float>(width) / static_cast<float>(height));
    }

    void processInput(GLFWwindow* window) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        const float speed = 5.0f * deltaTime;
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
        camera.rotate(dx * 0.1f, dy * 0.1f);
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

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Voxel Game 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
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
    if (!atlas.loadFromFile("assets/textures-atlas.png")) {
        std::cerr << "Texture atlas not found. Run from build directory.\n";
    }

    Chunk chunk(glm::ivec3(0, 0, 0));
    chunk.generateMesh();
    chunk.updateBuffers();

    camera.setAspect(static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight));

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(chunk.getPosition() * Chunk::CHUNK_SIZE));
        
        shader.setMat4("model", model);
        shader.setMat4("view", camera.viewMatrix());
        shader.setMat4("projection", camera.projectionMatrix());
        shader.setVec3("lightDir", glm::normalize(glm::vec3(0.5f, -1.0f, 0.3f)));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader.setInt("textureAtlas", 0);

        atlas.bind(0);
        chunk.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
