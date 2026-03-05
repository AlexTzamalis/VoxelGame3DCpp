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
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace {
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float lastX = Config::windowWidth / 2.0f;
    float lastY = Config::windowHeight / 2.0f;
    bool firstMouse = true;

    Camera camera(glm::vec3(0.0f, 60.0f, 5.0f));
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
        if (Config::currentState != GameState::PLAYING) return;
        
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
        if (Config::currentState != GameState::PLAYING) return;

        float speed = Config::playerSpeed;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) speed = Config::playerSprintSpeed;
        // 3x Boost mode for Creative/Spectator
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && Config::currentMode != GameMode::SURVIVAL) speed *= 3.0f;

        // Front direction varies by gamemode: Survival aligns to horizon floor (XZ), flying looks natively precisely anywhere
        glm::vec3 moveFront = (Config::currentMode == GameMode::SURVIVAL) 
            ? glm::normalize(glm::vec3(camera.front().x, 0, camera.front().z))
            : camera.front();

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.addVelocity(moveFront * speed);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.addVelocity(-moveFront * speed);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.addVelocity(-camera.right() * speed);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.addVelocity(camera.right() * speed);
            
        if (Config::currentMode != GameMode::SURVIVAL) {
            // Free flight up/down
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.addVelocity(glm::vec3(0.0f, speed, 0.0f));
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera.addVelocity(glm::vec3(0.0f, -speed, 0.0f));
        } else {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.jump(8.5f); // Fast Parabolic Jump
        }
        
        // Debug flight mode key if user gets stuck
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            camera.addVelocity(glm::vec3(0.0f, speed, 0.0f));
        }
        
        // GameMode Hotkey Handlers
        static bool f1Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
            if (!f1Pressed) { 
                Config::currentMode = (Config::currentMode == GameMode::SPECTATOR) ? GameMode::CREATIVE : GameMode::SPECTATOR; 
                f1Pressed = true; 
            }
        } else { f1Pressed = false; }
        
        static bool f2Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            if (!f2Pressed) { Config::currentMode = GameMode::CREATIVE; f2Pressed = true; }
        } else { f2Pressed = false; }
        
        static bool f3Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
            if (!f3Pressed) { Config::currentMode = GameMode::SURVIVAL; f3Pressed = true; }
        } else { f3Pressed = false; }
    }

    void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (Config::currentState != GameState::PLAYING) return;
        
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to init GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // ImGui Initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

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

        // Global State Toggles (ESCAPE key)
        static bool escapePressed = false;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (!escapePressed) {
                if (Config::currentState == GameState::PLAYING) {
                    Config::currentState = GameState::PAUSED;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                } else if (Config::currentState == GameState::PAUSED) {
                    Config::currentState = GameState::PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true;
                }
                escapePressed = true;
            }
        } else {
            escapePressed = false;
        }

        processInput(window);
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (Config::currentState == GameState::PLAYING) {
            chunkManager.update(camera.position());
        }

        if (Config::currentState == GameState::PLAYING || Config::currentState == GameState::PAUSED) {
            // Use a much brighter, natural-looking sky color
            glm::vec3 skyColor = glm::vec3(0.47f, 0.65f, 1.0f);
            glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            
            shader.setMat4("view", camera.viewMatrix());
            shader.setMat4("projection", camera.projectionMatrix());
            
            // Pass essential sky and fog values to the GPU
            shader.setVec3("cameraPos", camera.position());
            shader.setVec3("skyColor", skyColor);
            
            // Dynamically scale fog to the Render Distance!
            float renderDistBlocks = Config::renderDistance * 16.0f;
            shader.setFloat("fogEnd", renderDistBlocks);
            shader.setFloat("fogStart", renderDistBlocks * 0.6f); // Fog begins 60% of the way to the edge

            shader.setVec3("lightDir", glm::normalize(glm::vec3(0.5f, -1.0f, 0.3f)));
            shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
            shader.setInt("textureAtlas", 0);

            atlas.bind(0);
            
            if (Config::currentState == GameState::PLAYING) {
                // Calculate physics/AABB movement
                auto checkCollisionCall = [&](glm::vec3 minB, glm::vec3 maxB) -> bool {
                    // Epsilon shrink to cleanly slide across block faces without getting snagged at float borders
                    int startX = std::floor(minB.x + 0.01f);
                    int endX   = std::floor(maxB.x - 0.01f);
                    int startY = std::floor(minB.y + 0.01f);
                    int endY   = std::floor(maxB.y - 0.01f);
                    int startZ = std::floor(minB.z + 0.01f);
                    int endZ   = std::floor(maxB.z - 0.01f);
                    
                    for (int x = startX; x <= endX; x++) {
                        for (int y = startY; y <= endY; y++) {
                            for (int z = startZ; z <= endZ; z++) {
                                uint8_t voxel = globalChunkManager->getVoxelGlobal(x, y, z);
                                if (voxel > 1 && voxel != 5) return true; // Solid collision check
                            }
                        }
                    }
                    return false;
                };
                camera.applyPhysics(deltaTime, checkCollisionCall);

                // Update the Frustum boundary definitions based on currently moving camera coordinates
                camera.updateFrustum();
            }

            // Pass the actual ID of the shader, and the camera so it can perform visibility testing bounds!
            chunkManager.render(shader.id(), camera);
        } else {
            // Main Menu / Background Canvas (Dirt Brown color)
            glClearColor(0.12f, 0.09f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // GUI Rendering
        if (Config::currentState == GameState::MAIN_MENU) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 150, Config::windowHeight / 2.0f - 150), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_Always);
            ImGui::Begin("Voxel Game 3D", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Play World", ImVec2(280, 40))) {
                Config::currentState = GameState::PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            if (ImGui::Button("Create World", ImVec2(280, 40))) {
                Config::currentState = GameState::CREATE_WORLD;
            }
            if (ImGui::Button("Settings", ImVec2(280, 40))) {
                Config::currentState = GameState::SETTINGS;
            }
            ImGui::Separator();
            if (ImGui::Button("Quit Game", ImVec2(280, 40))) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::End();
        } 
        else if (Config::currentState == GameState::CREATE_WORLD) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 200, Config::windowHeight / 2.0f - 150), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Always);
            ImGui::Begin("Create New World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            static char worldName[64] = "My Beautiful World";
            ImGui::InputText("World Name", worldName, IM_ARRAYSIZE(worldName));
            
            static int seed = 1337;
            ImGui::InputInt("Seed", &seed);
            
            static int selectedMode = 1; // 0: Survival, 1: Creative, 2: Spectator
            ImGui::Combo("Game Mode", &selectedMode, "Survival\0Creative\0Spectator\0");
            
            ImGui::Separator();
            
            if (ImGui::Button("Create & Play!", ImVec2(380, 40))) {
                auto newMode = (selectedMode == 0) ? GameMode::SURVIVAL : ((selectedMode == 1) ? GameMode::CREATIVE : GameMode::SPECTATOR);
                Config::currentMode = newMode;
                // Note: Changing ChunkManager chunk seeding is planned for next iteration!
                
                Config::currentState = GameState::PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            if (ImGui::Button("Cancel", ImVec2(380, 40))) {
                Config::currentState = GameState::MAIN_MENU;
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::SETTINGS) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 200, Config::windowHeight / 2.0f - 200), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_Always);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            ImGui::Text("Video / Rendering");
            ImGui::SliderInt("Render Distance", &Config::renderDistance, 4, 32);
            ImGui::SliderInt("Vertical Distance", &Config::renderDistanceY, 2, 10);
            if (ImGui::SliderFloat("FOV", &Config::cameraFov, 60.0f, 110.0f)) {
                camera.setFov(Config::cameraFov);
            }
            
            ImGui::Separator();
            ImGui::Text("Controls");
            ImGui::SliderFloat("Player Speed", &Config::playerSpeed, 2.0f, 20.0f);
            ImGui::SliderFloat("Sprint Speed", &Config::playerSprintSpeed, 5.0f, 50.0f);
            ImGui::SliderFloat("Mouse Sensitivity", &Config::mouseSensitivity, 0.05f, 0.5f);
            
            ImGui::Separator();
            if (ImGui::Button("Back", ImVec2(380, 40))) {
                Config::currentState = GameState::MAIN_MENU;
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::PAUSED) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 150, Config::windowHeight / 2.0f - 100), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);
            ImGui::Begin("Game Paused", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Resume", ImVec2(280, 40))) {
                Config::currentState = GameState::PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            if (ImGui::Button("Save and Quit to Menu", ImVec2(280, 40))) {
                Config::currentState = GameState::MAIN_MENU;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
