#include "Camera.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "TextureAtlas.hpp"
#include "ChunkManager.hpp"
#include "Config.hpp"
#include "WorldManager.hpp"
#include "Inventory.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <sstream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace {
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float lastX = Config::windowWidth / 2.0f;
    float lastY = Config::windowHeight / 2.0f;
    bool firstMouse = true;

    Camera camera(glm::vec3(0.0f, 120.0f, 5.0f));
    ChunkManager* globalChunkManager = nullptr;
    Inventory playerInventory;

    bool raycast(glm::vec3 start, glm::vec3 direction, float maxDistance, glm::ivec3& hitPos, glm::ivec3& prevPos) {
        // Fast Voxel Traversal (DDA) Algorithm by Amanatides & Woo
        int stepX = (direction.x > 0) ? 1 : ((direction.x < 0) ? -1 : 0);
        int stepY = (direction.y > 0) ? 1 : ((direction.y < 0) ? -1 : 0);
        int stepZ = (direction.z > 0) ? 1 : ((direction.z < 0) ? -1 : 0);

        glm::vec3 tDelta(
            (stepX != 0) ? std::abs(1.0f / direction.x) : std::numeric_limits<float>::infinity(),
            (stepY != 0) ? std::abs(1.0f / direction.y) : std::numeric_limits<float>::infinity(),
            (stepZ != 0) ? std::abs(1.0f / direction.z) : std::numeric_limits<float>::infinity()
        );

        glm::ivec3 voxel(std::floor(start.x), std::floor(start.y), std::floor(start.z));
        
        glm::vec3 tMax(
            (stepX > 0) ? (voxel.x + 1.0f - start.x) * tDelta.x : (start.x - voxel.x) * tDelta.x,
            (stepY > 0) ? (voxel.y + 1.0f - start.y) * tDelta.y : (start.y - voxel.y) * tDelta.y,
            (stepZ > 0) ? (voxel.z + 1.0f - start.z) * tDelta.z : (start.z - voxel.z) * tDelta.z
        );

        int lastStepAxis = 0; // 0=X, 1=Y, 2=Z
        
        for (int i = 0; i < maxDistance * 3.0f; ++i) { // Bound to prevent infinite loops when pointing to void
            uint8_t currentBlock = globalChunkManager->getVoxelGlobal(voxel.x, voxel.y, voxel.z);
            if (currentBlock > 1 && currentBlock != 5) { // Stop at solid surfaces
                hitPos = voxel;
                prevPos = voxel;
                if (lastStepAxis == 0) prevPos.x -= stepX;
                else if (lastStepAxis == 1) prevPos.y -= stepY;
                else /* 2 */ prevPos.z -= stepZ;
                return true;
            }

            if (tMax.x < tMax.y) {
                if (tMax.x < tMax.z) {
                    voxel.x += stepX;
                    if (tMax.x > maxDistance) return false;
                    tMax.x += tDelta.x;
                    lastStepAxis = 0;
                } else {
                    voxel.z += stepZ;
                    if (tMax.z > maxDistance) return false;
                    tMax.z += tDelta.z;
                    lastStepAxis = 2;
                }
            } else {
                if (tMax.y < tMax.z) {
                    voxel.y += stepY;
                    if (tMax.y > maxDistance) return false;
                    tMax.y += tDelta.y;
                    lastStepAxis = 1;
                } else {
                    voxel.z += stepZ;
                    if (tMax.z > maxDistance) return false;
                    tMax.z += tDelta.z;
                    lastStepAxis = 2;
                }
            }
        }
        return false;
    }

    bool isIntersectingPlayer(glm::vec3 playerPos, glm::ivec3 blockPos) {
        float eyeOffsetY = 1.6f;
        glm::vec3 playerFeet = playerPos;
        playerFeet.y -= eyeOffsetY;
        
        float px1 = playerFeet.x - 0.3f; float px2 = playerFeet.x + 0.3f;
        float py1 = playerFeet.y;        float py2 = playerFeet.y + 1.8f;
        float pz1 = playerFeet.z - 0.3f; float pz2 = playerFeet.z + 0.3f;

        float bx1 = blockPos.x; float bx2 = blockPos.x + 1.0f;
        float by1 = blockPos.y; float by2 = blockPos.y + 1.0f;
        float bz1 = blockPos.z; float bz2 = blockPos.z + 1.0f;

        return (px1 < bx2 && px2 > bx1) &&
               (py1 < by2 && py2 > by1) &&
               (pz1 < bz2 && pz2 > bz1);
    }

    void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        if (Config::currentState != GameState::PLAYING) return;
        
        if (action == GLFW_PRESS && globalChunkManager != nullptr) {
            glm::ivec3 hitPos, prevPos;
            if (raycast(camera.position(), camera.front(), 8.0f, hitPos, prevPos)) {
                if (button == GLFW_MOUSE_BUTTON_LEFT) {
                    globalChunkManager->setVoxelGlobal(hitPos.x, hitPos.y, hitPos.z, 1); // Delete (Set Air)
                } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                    uint8_t holding = playerInventory.getSelectedBlock();
                    if (holding != 0) {
                        if (!isIntersectingPlayer(camera.position(), prevPos)) {
                            globalChunkManager->setVoxelGlobal(prevPos.x, prevPos.y, prevPos.z, holding); // Place
                        }
                    }
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
        // Hotbar selection map
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) playerInventory.setSelectedHotbar(0);
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) playerInventory.setSelectedHotbar(1);
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) playerInventory.setSelectedHotbar(2);
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) playerInventory.setSelectedHotbar(3);
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) playerInventory.setSelectedHotbar(4);
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) playerInventory.setSelectedHotbar(5);
        if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) playerInventory.setSelectedHotbar(6);
        if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) playerInventory.setSelectedHotbar(7);
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) playerInventory.setSelectedHotbar(8);

        // Open Inventory
        static bool ePressed = false;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            if (!ePressed) {
                playerInventory.isVisible = !playerInventory.isVisible;
                if (playerInventory.isVisible) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                } else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true;
                }
                ePressed = true;
            }
        } else { ePressed = false; }
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
        if (playerInventory.isVisible) return; // Freeze view if managing inventory!
        
        camera.rotate(dx * Config::mouseSensitivity, dy * Config::mouseSensitivity);
    }
}

int main() {
    WorldManager::init();
    Config::load(); // Load global user settings on start!

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

    // Disable raw driver VSync so we can process and sleep strictly mathematically below
    glfwSwapInterval(0); 

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
    Shader shadowShader("shaders/shadow.vert", "shaders/shadow.frag");
    
    // --- Set up Shadow Map FBO ---
    const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Prevents black shadowing from repeating outside the light frustum
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (shader.id() == 0 || shadowShader.id() == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!TextureAtlas::build("assets/texture_packs/default.zip")) {
        std::cerr << "Dynamic Texture Pack loading failed! Ensure default.zip exists in assets/texture_packs/\n";
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
                } else if (Config::currentState == GameState::COMMAND_INPUT) {
                    Config::currentState = GameState::PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true;
                }
                escapePressed = true;
            }
        } else {
            escapePressed = false;
        }

        // Command System Input (Slash key)
        static bool slashPressed = false;
        if (glfwGetKey(window, GLFW_KEY_SLASH) == GLFW_PRESS) {
            if (!slashPressed && Config::currentState == GameState::PLAYING && !playerInventory.isVisible) {
                Config::currentState = GameState::COMMAND_INPUT;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                slashPressed = true;
            }
        } else {
            slashPressed = false;
        }

        processInput(window);
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (Config::currentState == GameState::PLAYING) {
            chunkManager.update(camera.position());
        }

        if (Config::currentState == GameState::PLAYING || Config::currentState == GameState::PAUSED || Config::currentState == GameState::COMMAND_INPUT) {
            // Day-Night Cycle calculations
            float timeVal = static_cast<float>(glfwGetTime());
            float dayPhase = timeVal * Config::dayTimeSpeed;
            
            // Vector pointing TOWARDS the sun
            glm::vec3 sunDir = glm::normalize(glm::vec3(cos(dayPhase), sin(dayPhase), 0.3f));
            float sunHeight = sunDir.y;

            glm::vec3 daySky(0.47f, 0.65f, 1.0f);
            glm::vec3 nightSky(0.01f, 0.02f, 0.05f); // Deep Space Blue
            glm::vec3 sunsetSky(0.9f, 0.4f, 0.3f);   // Vibrant Orange
            
            glm::vec3 skyColor;
            if (sunHeight > 0.2f) skyColor = daySky;
            else if (sunHeight > 0.0f) skyColor = glm::mix(sunsetSky, daySky, sunHeight / 0.2f);
            else if (sunHeight > -0.2f) skyColor = glm::mix(nightSky, sunsetSky, (sunHeight + 0.2f) / 0.2f);
            else skyColor = nightSky;

            glm::vec3 lightColor;
            if (sunHeight > 0.2f) lightColor = glm::vec3(1.2f, 1.1f, 1.0f); // Bright Midday
            else if (sunHeight > 0.0f) lightColor = glm::mix(glm::vec3(1.2f, 0.5f, 0.2f), glm::vec3(1.2f, 1.1f, 1.0f), sunHeight / 0.2f); // Golden Hour
            else lightColor = glm::vec3(0.15f, 0.2f, 0.35f); // Piercing Moonlight tint

            // Use the moon as the light source effectively if the sun is down
            glm::vec3 shaderLightDir = sunHeight > 0.0f ? sunDir : glm::vec3(-sunDir.x, -sunDir.y, -sunDir.z); 

            // Detect if camera is physically touching water
            int camX = std::floor(camera.position().x);
            int camY = std::floor(camera.position().y);
            int camZ = std::floor(camera.position().z);
            bool isUnderwater = (chunkManager.getVoxelGlobal(camX, camY, camZ) == 5);

            if (isUnderwater && Config::enableShaders) {
                skyColor = glm::vec3(0.04f, 0.12f, 0.35f); // Instant deep oceanic blue skybox
            }

            // PASS 1: SHADOW MAP RENDERING
            float shadowDist = Config::renderDistance * 16.0f; 
            glm::mat4 lightProjection = glm::ortho(-shadowDist, shadowDist, -shadowDist, shadowDist, -shadowDist * 2.0f, shadowDist * 3.0f);
            
            glm::vec3 lightTarget = camera.position(); 
            glm::vec3 lightPos = lightTarget + (shaderLightDir * shadowDist);
            
            glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;

            if (Config::enableShadows && Config::enableShaders) {
                glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
                glClear(GL_DEPTH_BUFFER_BIT); // Depth buffer only
                
                shadowShader.use();
                shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
                shadowShader.setFloat("time", timeVal);
                
                glActiveTexture(GL_TEXTURE0);
                TextureAtlas::bind(0);
                shadowShader.setInt("textureAtlas", 0);
                
                // Bypass frustum checks entirely so chunks behind the camera cast shadows into frame,
                // but heavily cull chunks outside the shadow texture bounds so we don't destroy CPU limit!
                chunkManager.render(shadowShader.id(), camera, true, shadowDist * 1.5f);
            }
            
            // PASS 2: NORMAL RENDERING
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            int fbW, fbH;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            glViewport(0, 0, fbW, fbH);

            glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            
            shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
            shader.setInt("enableShaders", Config::enableShaders ? 1 : 0);
            shader.setInt("enableShadows", Config::enableShadows ? 1 : 0);
            
            shader.setMat4("view", camera.viewMatrix());
            shader.setMat4("projection", camera.projectionMatrix());
            
            shader.setFloat("time", timeVal);
            shader.setInt("isUnderwater", isUnderwater ? 1 : 0);
            shader.setVec3("cameraPos", camera.position());
            shader.setVec3("skyColor", skyColor);
            
            // Render Fog Distances
            float renderDistBlocks = Config::renderDistance * 16.0f;
            if (isUnderwater) {
                shader.setFloat("fogStart", 0.0f);
                shader.setFloat("fogEnd", 24.0f); // Quick fade when exploring oceans
            } else {
                shader.setFloat("fogStart", renderDistBlocks - 32.0f); // Minimalistic chunk-loading fog on the X-Z border
                shader.setFloat("fogEnd", renderDistBlocks);
            }

            shader.setVec3("lightDir", shaderLightDir);
            shader.setVec3("lightColor", lightColor);
            
            glActiveTexture(GL_TEXTURE0);
            TextureAtlas::bind(0);
            shader.setInt("textureAtlas", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            shader.setInt("shadowMap", 1);
            
            if (Config::currentState == GameState::PLAYING) {
                // Determine if chunk we are inside is fully generated
                int cx = std::floor(camera.position().x / Chunk::CHUNK_SIZE);
                int cz = std::floor(camera.position().z / Chunk::CHUNK_SIZE);
                
                bool isLoaded = chunkManager.isChunkColumnLoaded(cx, cz);
                if (isLoaded || camera.position().y > 80.0f) { // Added height bypass to not get stuck above generation!
                    // Safe to apply gravity and collision sweeps without sinking into void
                    auto checkCollisionCall = [&](glm::vec3 minB, glm::vec3 maxB) -> bool {
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
                }

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

        // Draw Player Live UI Overlay inside playing mode loop
        if (Config::currentState == GameState::PLAYING) {
            
            // Draw simple FPS counter exactly in top left
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f); // Transparent dark background
            ImGui::Begin("FPS_Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
            ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
            ImGui::End();

            // perfectly centered mathematical Crosshair
            ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();
            ImVec2 center(Config::windowWidth * 0.5f, Config::windowHeight * 0.5f);
            float crosshairSize = 10.0f;
            bgDrawList->AddLine(ImVec2(center.x - crosshairSize, center.y), ImVec2(center.x + crosshairSize, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
            bgDrawList->AddLine(ImVec2(center.x, center.y - crosshairSize), ImVec2(center.x, center.y + crosshairSize), IM_COL32(255, 255, 255, 200), 2.0f);

            // Hotbar Rendering
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - (9.0f * 50.0f) / 2.0f, Config::windowHeight - 80.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(9.0f * 50.0f, 65.0f), ImGuiCond_Always);
            ImGui::Begin("Hotbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            
            for (int i = 0; i < 9; ++i) {
                if (i > 0) ImGui::SameLine();
                uint8_t item = playerInventory.slots[i].itemID;
                
                // Active slot styling via Frame changes
                if (playerInventory.selectedHotbarIndex == i) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.8f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                }
                
                std::string lbl = std::to_string(item) + "##btn" + std::to_string(i);
                if (item == 0) lbl = " ##btn" + std::to_string(i); // Show space instead of 0 for air
                
                if (ImGui::Button(lbl.c_str(), ImVec2(40, 40))) {
                    playerInventory.setSelectedHotbar(i);
                }
                
                if (playerInventory.selectedHotbarIndex == i) {
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                }
            }
            ImGui::End();

            // Draw full floating 27-slot Inventory Modal if E is Pressed
            if (playerInventory.isVisible) {
                ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 150, Config::windowHeight / 2.0f - 100), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);
                ImGui::Begin("Inventory", &playerInventory.isVisible, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
                
                for (int slot = 9; slot < 36; ++slot) {
                    if ((slot - 9) % 9 != 0) ImGui::SameLine();
                    
                    uint8_t item = playerInventory.slots[slot].itemID;
                    std::string lbl = std::to_string(item) + "##inv" + std::to_string(slot);
                    if (item == 0) lbl = " ##inv" + std::to_string(slot);
                    
                    ImGui::Button(lbl.c_str(), ImVec2(24, 24));
                    // Basic placeholder mechanics - drag drop can be added next loop
                }
                
                ImGui::End();
                
                // Fix cursor visibility when manually X-ing out of the window or toggling boolean
                if (!playerInventory.isVisible) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true;
                }
            }
        }

        // GUI Rendering
        if (Config::currentState == GameState::MAIN_MENU) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 150, Config::windowHeight / 2.0f - 150), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_Always);
            ImGui::Begin("Voxel Game 3D", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Singleplayer", ImVec2(280, 40))) {
                Config::currentState = GameState::WORLD_SELECT;
            }
            if (ImGui::Button("Settings", ImVec2(280, 40))) {
                Config::currentState = GameState::SETTINGS;
            }
            ImGui::Separator();
            if (ImGui::Button("Quit Game", ImVec2(280, 40))) {
                Config::save();
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::End();
        } 
        else if (Config::currentState == GameState::WORLD_SELECT) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 250, Config::windowHeight / 2.0f - 200), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Always);
            ImGui::Begin("Select World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            auto worlds = WorldManager::getSavedWorlds();
            
            ImGui::BeginChild("WorldList", ImVec2(0, 280), true);
            if (worlds.empty()) {
                ImGui::TextDisabled("No worlds found. Create a new one!");
            } else {
                for (const auto& w : worlds) {
                    if (ImGui::TreeNodeEx(w.displayName.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Mode: %d | Type: %d | Time Played: %llds", (int)w.mode, w.worldType, w.timePlayedSeconds);
                        ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                        
                        std::string playLabel = "Play##" + w.folderName;
                        if (ImGui::Button(playLabel.c_str(), ImVec2(80, 25))) {
                            chunkManager.clear();
                            WorldManager::loadWorld(w.folderName);
                            
                            glm::vec3 savedPos; float pitch, yaw;
                            if (WorldManager::loadPlayer(savedPos, pitch, yaw)) {
                                camera.setPosition(savedPos);
                                camera.setPitch(pitch);
                                camera.setYaw(yaw);
                                camera.updateVectors();
                            } else {
                                camera.setPosition(glm::vec3(0.0f, 120.0f, 5.0f)); 
                            }
                            
                            Config::currentState = GameState::PLAYING;
                            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                            firstMouse = true;
                        }
                    }
                }
            }
            ImGui::EndChild();
            
            if (ImGui::Button("Create New World", ImVec2(230, 40))) {
                Config::currentState = GameState::CREATE_WORLD;
            }
            ImGui::SameLine();
            if (ImGui::Button("Back to Menu", ImVec2(230, 40))) {
                Config::currentState = GameState::MAIN_MENU;
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::CREATE_WORLD) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 200, Config::windowHeight / 2.0f - 180), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400, 340), ImGuiCond_Always);
            ImGui::Begin("Create New World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            static char worldName[64] = "My Beautiful World";
            ImGui::InputText("World Name", worldName, IM_ARRAYSIZE(worldName));
            
            static int seed = 1337;
            ImGui::InputInt("Seed", &seed);
            
            static int selectedMode = 1; // 0: Survival, 1: Creative, 2: Spectator
            ImGui::Combo("Game Mode", &selectedMode, "Survival\0Creative\0Spectator\0");
            
            static int selectedTerrain = 0; // 0: Default, 1: Flat, 2: Skyblock
            ImGui::Combo("Terrain Type", &selectedTerrain, "Default\0Flat World\0Skyblock\0");
            
            ImGui::Separator();
            
            if (ImGui::Button("Create & Play!", ImVec2(380, 40))) {
                auto newMode = (selectedMode == 0) ? GameMode::SURVIVAL : ((selectedMode == 1) ? GameMode::CREATIVE : GameMode::SPECTATOR);
                
                WorldMetadata newMeta;
                newMeta.displayName = std::string(worldName);
                if (newMeta.displayName.empty()) newMeta.displayName = "Unnamed_World";
                
                // Keep file names path safe
                newMeta.folderName = newMeta.displayName; 
                std::replace(newMeta.folderName.begin(), newMeta.folderName.end(), ' ', '_'); 
                newMeta.folderName += "_" + std::to_string(std::abs(seed));
                
                newMeta.seed = seed;
                newMeta.mode = newMode;
                newMeta.worldType = selectedTerrain;
                newMeta.creationDate = getCurrentTimeMs();
                newMeta.lastPlayed = getCurrentTimeMs();
                newMeta.timePlayedSeconds = 0;
                
                WorldManager::createWorld(newMeta);
                
                // Clear any chunks from previously playing!
                chunkManager.clear();
                WorldManager::loadWorld(newMeta.folderName);
                
                glm::vec3 savedPos; float pitch, yaw;
                if (WorldManager::loadPlayer(savedPos, pitch, yaw)) {
                    camera.setPosition(savedPos);
                    camera.setPitch(pitch);
                    camera.setYaw(yaw);
                    camera.updateVectors();
                } else {
                    camera.setPosition(glm::vec3(0.0f, 120.0f, 5.0f)); // Initial spawn location (Falling safely)
                }
                
                WorldManager::savePlayer(camera.position(), camera.pitch(), camera.yaw());
                
                Config::currentState = GameState::PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            if (ImGui::Button("Cancel", ImVec2(380, 40))) {
                Config::currentState = GameState::WORLD_SELECT;
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::SETTINGS) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 250, Config::windowHeight / 2.0f - 200), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Always);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            if (ImGui::BeginTabBar("SettingsTabs")) {
                if (ImGui::BeginTabItem("Video")) {
                    ImGui::Spacing();
                    ImGui::SliderInt("Render Distance", &Config::renderDistance, 4, 32);
                    ImGui::SliderInt("Vertical Distance", &Config::renderDistanceY, 2, 10);
                    ImGui::SliderInt("Max FPS", &Config::fpsCap, 30, 240);
                    if (ImGui::SliderFloat("FOV", &Config::cameraFov, 60.0f, 110.0f)) {
                        camera.setFov(Config::cameraFov);
                    }
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Shaders")) {
                    ImGui::Spacing();
                    
                    bool changedShaders = ImGui::Checkbox("Enable Advanced Shaders (Water, Fog, Radiosity)", &Config::enableShaders);
                    
                    if (Config::enableShaders) {
                        ImGui::Checkbox("Enable Dynamic Cascaded Shadows", &Config::enableShadows);
                    } else {
                        ImGui::BeginDisabled();
                        bool f = false; ImGui::Checkbox("Enable Dynamic Cascaded Shadows", &f);
                        ImGui::EndDisabled();
                    }
                    
                    // Allow UI to tweak Day / Night Speed dynamically
                    // Display it slightly easier to read (multiplier over 24h)
                    float currentHours = (0.0021816f / Config::dayTimeSpeed) * 0.8f; // ~1h baseline display
                    if (ImGui::SliderFloat("Day Speed Mult", &Config::dayTimeSpeed, 0.0001f, 0.1f, "%.5f")) {
                        // Dynamically updates gameplay lighting loop above!
                    }
                    ImGui::TextDisabled("Default is 0.00218 (48 real-life minutes for 24h)");
                    
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::Spacing();
                    ImGui::SliderFloat("Player Speed", &Config::playerSpeed, 2.0f, 20.0f);
                    ImGui::SliderFloat("Sprint Speed", &Config::playerSprintSpeed, 5.0f, 50.0f);
                    ImGui::SliderFloat("Mouse Sensitivity", &Config::mouseSensitivity, 0.05f, 0.5f);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            ImGui::Separator();
            if (ImGui::Button("Save & Back", ImVec2(480, 40))) {
                Config::save();
                // If we accessed settings while paused, go back to pause menu!
                if (!firstMouse) Config::currentState = GameState::PAUSED;
                else Config::currentState = GameState::MAIN_MENU;
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
            if (ImGui::Button("Settings", ImVec2(280, 40))) {
                Config::currentState = GameState::SETTINGS;
            }
            if (ImGui::Button("Save and Quit to Menu", ImVec2(280, 40))) {
                WorldManager::savePlayer(camera.position(), camera.pitch(), camera.yaw());
                WorldManager::updatePlayTime();
                chunkManager.clear(); // Flushes all modified chunks to disk securely!
                Config::save(); // Persist video/audio settings as well
                Config::currentState = GameState::MAIN_MENU;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::COMMAND_INPUT) {
            ImGui::SetNextWindowPos(ImVec2(10, Config::windowHeight - 50), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(600, 40), ImGuiCond_Always);
            ImGui::Begin("Command input", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            
            static char cmdBuf[256] = "";
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
                cmdBuf[0] = '/'; 
                cmdBuf[1] = '\0';
            }
            if (ImGui::InputText("##cmd", cmdBuf, IM_ARRAYSIZE(cmdBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string cmdStr(cmdBuf);
                std::stringstream ss(cmdStr);
                std::string token;
                ss >> token;
                
                if (token == "/time") {
                    double newTime;
                    if (ss >> newTime) {
                         glfwSetTime(newTime / Config::dayTimeSpeed); // Quick hack to set internal day phase
                    }
                } else if (token == "/speed") {
                    float spd;
                    if (ss >> spd) {
                        Config::playerSpeed = spd;
                        Config::playerSprintSpeed = spd * 1.5f;
                    }
                } else if (token == "/teleport") {
                    float px, py, pz;
                    if (ss >> px >> py >> pz) {
                        camera.setPosition(glm::vec3(px, py, pz));
                    }
                } else if (token == "/locate") {
                    std::string entity;
                    ss >> entity;
                    std::cout << "Locating " << entity << "..." << "\n";
                    // Just print coordinate for now as placeholder
                }
                
                cmdBuf[0] = '\0';
                Config::currentState = GameState::PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // Manual Frame Pacing & Latency Control
        float frameTime = static_cast<float>(glfwGetTime()) - currentFrame;
        float frameTarget = 1.0f / static_cast<float>(Config::fpsCap);
        
        if (frameTime < frameTarget) {
            float sleepTime = frameTarget - frameTime;
            // Sleep the core CPU thread to drop rendering cycles and smooth out GPU stutter!
            std::this_thread::sleep_for(std::chrono::duration<float>(sleepTime));
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
