#include "Camera.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
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

    // Cube: 24 vertices (4 per face), position + normal + uv
    // Arranged in CCW order from outside: top-left, bot-left, bot-right, top-right
    float cubeVertices[] = {
        // +X 
        0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top-left
        0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bot-left
        0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f, // bot-right
        0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top-right

        // -X 
       -0.5f,  0.5f, -0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 1.0f,
       -0.5f, -0.5f, -0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
       -0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
       -0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   1.0f, 1.0f,

        // +Y 
       -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
       -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,

        // -Y 
       -0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f,   0.0f, 1.0f,
       -0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f,   0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f,   1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f,   1.0f, 1.0f,

        // +Z 
       -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
       -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,

        // -Z 
        0.5f,  0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   0.0f, 1.0f,
        0.5f, -0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   0.0f, 0.0f,
       -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   1.0f, 0.0f,
       -0.5f,  0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f,
    };
    unsigned int cubeIndices[] = {
         0,  1,  2,  0,  2,  3,
         4,  5,  6,  4,  6,  7,
         8,  9, 10,  8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    unsigned int vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

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
        model = glm::rotate(model, currentFrame * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        shader.setMat4("model", model);
        shader.setMat4("view", camera.viewMatrix());
        shader.setMat4("projection", camera.projectionMatrix());
        shader.setVec2("atlasTileSize", glm::vec2(1.0f / 16.0f, 1.0f / 16.0f));
        shader.setVec2("atlasOffset", glm::vec2(1.0f / 16.0f, 15.0f / 16.0f)); // Point to a grass block or dirt
        shader.setVec3("lightDir", glm::vec3(0.5f, -1.0f, 0.3f));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader.setInt("textureAtlas", 0);

        atlas.bind(0);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
