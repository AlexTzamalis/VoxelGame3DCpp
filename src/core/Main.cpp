#include "renderer/Camera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"
#include "renderer/TextureAtlas.hpp"
#include "renderer/PlayerRenderer.hpp"
#include "world/ChunkManager.hpp"
#include "core/Config.hpp"
#include "world/WorldManager.hpp"
#include "world/BlockRegistry.hpp"
#include "game/Inventory.hpp"
#include "game/InputManager.hpp"
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
    char createWorldNameBuf[64] = "New World 1";

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
        float eyeOffsetY = 1.62f;
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
        
        if (action == GLFW_PRESS && globalChunkManager != nullptr && !playerInventory.isVisible) {
            glm::ivec3 hitPos, prevPos;
            float reach = (Config::currentMode == GameMode::SURVIVAL) ? 5.0f : 8.0f;
            if (raycast(camera.position(), camera.front(), reach, hitPos, prevPos)) {
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
        if (height == 0 || width == 0) return; // Prevent aspect ratio division by zero / assertion failure on minimize!
        glViewport(0, 0, width, height);
        Config::windowWidth = width;
        Config::windowHeight = height;
        camera.setAspect(static_cast<float>(width) / static_cast<float>(height));
    }

    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        if (Config::currentState != GameState::PLAYING) return;
        
        // If zooming, scroll adjusts zoom level instead of hotbar
        if (camera.isZooming()) {
            camera.adjustZoom(static_cast<float>(yoffset));
            return;
        }
        
        // Otherwise, scroll changes hotbar selection
        int current = playerInventory.selectedHotbarIndex;
        current -= static_cast<int>(yoffset); // scroll up = previous slot
        if (current < 0) current = 8;
        if (current > 8) current = 0;
        playerInventory.setSelectedHotbar(current);
    }

    void processInput(GLFWwindow* window) {
        if (Config::currentState != GameState::PLAYING) return;

        float speed = Config::playerSpeed;
        
        bool sneaking = InputManager::isPressed(InputAction::DESCEND, window) && Config::currentMode == GameMode::SURVIVAL;
        bool sprinting = InputManager::isPressed(InputAction::SPRINT, window) && !sneaking;
        
        if (sneaking) {
            speed = Config::playerSneakSpeed;
        } else if (sprinting) {
            speed = Config::playerSprintSpeed;
            // Extra boost when flying
            if (Config::currentMode != GameMode::SURVIVAL) speed *= 2.0f;
        }
        
        camera.setSprinting(sprinting);

        // Zoom toggle (hold key)
        camera.setZooming(InputManager::isPressed(InputAction::ZOOM, window));

        // Horizontal movement always uses horizon-aligned front (XZ plane)
        // This prevents the camera pitch from affecting walk/fly speed
        glm::vec3 moveFront = glm::normalize(glm::vec3(camera.front().x, 0, camera.front().z));
        glm::vec3 inputDir(0.0f);

        if (InputManager::isPressed(InputAction::MOVE_FORWARD, window)) inputDir += moveFront;
        if (InputManager::isPressed(InputAction::MOVE_BACK, window)) inputDir -= moveFront;
        if (InputManager::isPressed(InputAction::MOVE_LEFT, window)) inputDir -= camera.right();
        if (InputManager::isPressed(InputAction::MOVE_RIGHT, window)) inputDir += camera.right();
        
        if (glm::length(inputDir) > 0.01f) {
            inputDir = glm::normalize(inputDir);
            camera.addVelocity(inputDir * speed);
        }
            
        if (Config::currentMode != GameMode::SURVIVAL) {
            // Flying: Space = up, Shift = down (Minecraft creative controls)
            if (InputManager::isPressed(InputAction::JUMP, window)) camera.addVelocity(glm::vec3(0.0f, speed, 0.0f));
            if (InputManager::isPressed(InputAction::DESCEND, window)) camera.addVelocity(glm::vec3(0.0f, -speed, 0.0f));
        } else {
            if (InputManager::isPressed(InputAction::JUMP, window)) camera.jump(8.8f);
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

        static bool f5Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
            if (!f5Pressed) { camera.toggleViewMode(); f5Pressed = true; }
        } else { f5Pressed = false; }

        // Hotbar selection via number keys
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
        if (InputManager::wasJustPressed(InputAction::INVENTORY, window)) {
            playerInventory.isVisible = !playerInventory.isVisible;
            if (playerInventory.isVisible) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
        }
    }

    void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (Config::currentState != GameState::PLAYING) return;
        
        // Let ImGui capture mouse entirely if inventory is open
        if (playerInventory.isVisible) {
            firstMouse = true;
            return;
        }
        
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

    void applyGraphicsPreset(GraphicsPreset preset) {
        Config::graphicsPreset = preset;
        if (preset == GraphicsPreset::CUSTOM) return;

        // Reset to reasonable defaults first
        Config::enableShaders = true;
        Config::enableShadows = true;
        Config::enableFog = true;
        Config::enableDirectionalFaceShading = true;
        Config::enableClouds = true;
        Config::enableLeafWind = true;
        Config::waterMode = 1;

        switch (preset) {
            case GraphicsPreset::LOW:
                Config::renderDistance = 6;
                Config::enableShadows = false;
                Config::enableClouds = true; // Keep clouds for visual
                Config::cloudQuality = 8;
                Config::cloudDensity = 0.35f;
                Config::enableFog = true;
                Config::fogDensity = 0.015f;
                Config::waterMode = 0; // Simple flat water
                Config::waterTransparency = 0.4f;
                Config::shadowMapSize = 512;
                Config::godRaysIntensity = 0.0f;
                Config::enableLOD = false;
                Config::sunSize = 0.01f;
                Config::starDensity = 0.2f;
                break;
            case GraphicsPreset::MEDIUM:
                Config::renderDistance = 12;
                Config::enableShadows = true;
                Config::shadowSoftness = 0.5f;
                Config::enableClouds = true;
                Config::cloudQuality = 12;
                Config::cloudDensity = 0.45f;
                Config::waterMode = 1;
                Config::waterTransparency = 0.6f;
                Config::enableWaterReflections = true;
                Config::shadowMapSize = 1024;
                Config::godRaysIntensity = 0.1f;
                Config::enableLOD = true;
                Config::lodDistance = 128;
                Config::sunSize = 0.02f;
                Config::starDensity = 0.5f;
                break;
            case GraphicsPreset::HIGH:
                Config::renderDistance = 20;
                Config::enableShadows = true;
                Config::shadowSoftness = 1.0f;
                Config::enableClouds = true;
                Config::cloudQuality = 24;
                Config::cloudDensity = 0.55f;
                Config::waterMode = 1;
                Config::waterTransparency = 0.75f;
                Config::enableWaterReflections = true;
                Config::shadowMapSize = 2048;
                Config::godRaysIntensity = 0.25f;
                Config::enableLOD = true;
                Config::lodDistance = 256;
                Config::sunSize = 0.015f;
                Config::ultraMode = false; // Disable volumetric rays for performance
                Config::starDensity = 1.0f;
                break;
            case GraphicsPreset::ULTRA:
                Config::renderDistance = 32;
                Config::enableShadows = true;
                Config::shadowSoftness = 1.5f;
                Config::enableClouds = true;
                Config::cloudQuality = 48;
                Config::cloudDensity = 0.65f;
                Config::waterMode = 1;
                Config::waterTransparency = 0.85f;
                Config::enableWaterReflections = true;
                Config::shadowMapSize = 4096;
                Config::godRaysIntensity = 0.45f;
                Config::enableLOD = true;
                Config::lodDistance = 512;
                Config::sunSize = 0.018f;
                Config::starDensity = 2.0f;
                Config::milkyWayIntensity = 0.8f;
                break;
            default: break;
        }
    }
}

int main() {
    InputManager::init();
    WorldManager::init();
    BlockRegistry::init();
    Config::load(); // Load global user settings on start!

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    
    // Automatically clamp to Native if we don't have proper resolution config
    if (Config::windowWidth == 1280 && Config::isFullscreen) {
        Config::windowWidth = mode->width;
        Config::windowHeight = mode->height;
    }

    GLFWwindow* window = nullptr;
    if (Config::isFullscreen) {
        window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, "Voxel Game 3D", primaryMonitor, nullptr);
    } else {
        window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, "Voxel Game 3D", nullptr, nullptr);
    }
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
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
    Shader cloudShader("shaders/cloud.vert", "shaders/cloud.frag");
    Shader playerShader("shaders/player.vert", "shaders/player.frag");
    
    PlayerRenderer playerRenderer;
    // Fullscreen quad for cloud rendering
    float quadVerts[] = { -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, 1.0f };
    unsigned int cloudVAO, cloudVBO;
    glGenVertexArrays(1, &cloudVAO);
    glGenBuffers(1, &cloudVBO);
    glBindVertexArray(cloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    
    // --- Set up Shadow Map FBO ---
    unsigned int shadowTexSize = 2048; // Will be recreated if config changes
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
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
    
    Shader solidShader("shaders/solid.vert", "shaders/solid.frag");
    
    // --- Set up wireframe block outline ---
    unsigned int outlineVao, outlineVbo;
    glGenVertexArrays(1, &outlineVao);
    glGenBuffers(1, &outlineVbo);
    
    float outlineVertices[] = {
        0,0,0, 1,0,0,  1,0,0, 1,0,1,  1,0,1, 0,0,1,  0,0,1, 0,0,0, // Bottom
        0,1,0, 1,1,0,  1,1,0, 1,1,1,  1,1,1, 0,1,1,  0,1,1, 0,1,0, // Top
        0,0,0, 0,1,0,  1,0,0, 1,1,0,  1,0,1, 1,1,1,  0,0,1, 0,1,1  // Columns
    };
    
    glBindVertexArray(outlineVao);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVertices), outlineVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::cerr << "[Trace] Building Texture Atlas...\n";
    if (!TextureAtlas::build("assets/texture_packs/default.zip")) {
        std::cerr << "Dynamic Texture Pack loading failed! Ensure default.zip exists in assets/texture_packs/\n";
    }

    std::cerr << "[Trace] Initializing PlayerRenderer...\n";
    playerRenderer.init();

    std::cerr << "[Trace] Creating ChunkManager...\n";
    ChunkManager chunkManager;
    globalChunkManager = &chunkManager;
    
    std::cerr << "[Trace] Configuring Camera...\n";

    camera.setRaycastFunc([](glm::vec3 start, glm::vec3 dir, float maxDist) -> float {
        glm::ivec3 hitPos, prevPos;
        if (raycast(start, dir, maxDist, hitPos, prevPos)) {
            // Found a solid block. Return the distance to the intersection boundary.
            // A simple approximation is the distance to the center of the block minus ~0.5.
            return glm::distance(start, glm::vec3(hitPos) + glm::vec3(0.5f)) - 0.5f;
        }
        return -1.0f; // No hit
    });

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

        // Command System Input (configurable key)
        if (InputManager::wasJustPressed(InputAction::COMMAND, window)) {
            if (Config::currentState == GameState::PLAYING && !playerInventory.isVisible) {
                Config::currentState = GameState::COMMAND_INPUT;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        processInput(window);
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (Config::currentState == GameState::PLAYING) {
            chunkManager.update(camera);
        }

        if (Config::currentState == GameState::PLAYING || Config::currentState == GameState::PAUSED || Config::currentState == GameState::COMMAND_INPUT) {
            // Day-Night Cycle calculations
            float timeVal = static_cast<float>(glfwGetTime());
            float dayPhase = timeVal * Config::dayTimeSpeed;
            
            // Vector pointing TOWARDS the sun - GLOBAL axis, not relative to player
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
            else lightColor = glm::vec3(0.1f, 0.12f, 0.2f) + glm::vec3(Config::ambientBrightness * 0.3f); // Moonlight + ambient boost

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
            float shadowDist = std::min(Config::renderDistance * 16.0f, 256.0f); // Cap shadow distance  
            glm::mat4 lightProjection = glm::ortho(-shadowDist, shadowDist, -shadowDist, shadowDist, -shadowDist * 2.0f, shadowDist * 3.0f);
            
            // Shadow light target follows camera smoothly
            glm::vec3 lightTarget = camera.position();
            glm::vec3 lightPos = lightTarget + (shaderLightDir * shadowDist);
            
            glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;
            
            // TEXEL SNAPPING: Prevents shadow flickering/shimmer when camera moves
            // Snap the shadow map origin to texel boundaries so shadows don't shift sub-pixel
            {
                glm::vec4 shadowOrigin = lightSpaceMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                shadowOrigin *= SHADOW_WIDTH / 2.0f;
                glm::vec4 roundedOrigin = glm::round(shadowOrigin);
                glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
                roundOffset /= SHADOW_WIDTH / 2.0f;
                lightProjection[3][0] += roundOffset.x;
                lightProjection[3][1] += roundOffset.y;
                lightSpaceMatrix = lightProjection * lightView;
            }

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
            
            // Granular shader uniforms
            shader.setFloat("shadowIntensity", Config::shadowIntensity);
            shader.setFloat("shadowSoftness", Config::shadowSoftness);
            shader.setFloat("shadowBias", Config::shadowBias);

            shader.setInt("enableFog", Config::enableFog ? 1 : 0);
            shader.setVec3("fogColor", Config::fogColor);
            shader.setFloat("fogHeightFalloff", Config::fogHeightFalloff);

            shader.setInt("enableFaceShading", Config::enableDirectionalFaceShading ? 1 : 0);
            shader.setFloat("ambientBrightness", Config::ambientBrightness);
            shader.setInt("waterMode", Config::waterMode);
            shader.setFloat("waterTransparency", Config::waterTransparency);
            shader.setFloat("waterWaveSpeed", Config::waterWaveSpeed);
            shader.setFloat("waterWaveHeight", Config::waterWaveHeight);
            shader.setInt("enableWaterReflections", Config::enableWaterReflections ? 1 : 0);
            
            shader.setInt("enableLeafWind", Config::enableLeafWind ? 1 : 0);
            shader.setInt("ultraMode", Config::ultraMode ? 1 : 0);
            shader.setFloat("saturation", Config::saturation);
            shader.setFloat("contrast", Config::contrast);
            
            // Atmosphere
            shader.setFloat("sunSize", Config::sunSize);
            shader.setFloat("sunIntensity", Config::sunIntensity);
            shader.setVec3("sunColor", Config::sunColor);
            shader.setFloat("moonSize", Config::moonSize);
            shader.setFloat("moonIntensity", Config::moonIntensity);
            shader.setFloat("godRaysIntensity", Config::godRaysIntensity);
            shader.setFloat("starDensity", Config::starDensity);
            shader.setFloat("milkyWayIntensity", Config::milkyWayIntensity);

            // Dynamic Fog scaling for Distant Horizons
            float fogE = float(Config::renderDistance + Config::lodDistance) * 16.0f;
            if (!Config::enableLOD) fogE = float(Config::renderDistance + 1) * 16.0f;
            
            // Safety: Never let fog end be too close
            fogE = std::max(fogE, 64.0f);

            if (isUnderwater) {
                shader.setFloat("fogStart", 0.0f);
                shader.setFloat("fogEnd", 32.0f); // Slightly more visibility
            } else {
                shader.setFloat("fogStart", fogE * 0.15f); // Constant atmospheric haze starting close
                shader.setFloat("fogEnd", fogE);
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
                int cy = std::floor(camera.position().y / Chunk::CHUNK_SIZE);
                int cz = std::floor(camera.position().z / Chunk::CHUNK_SIZE);
                
                bool isLoaded = chunkManager.isChunkColumnLoaded(cx, cy, cz);
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
                                    // Solid collision check: Ignore Air (1), Water (5), and Foliage (31-34)
                                    if (voxel > 1 && voxel != 5 && (voxel < 31 || voxel > 34)) return true; 
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

            chunkManager.render(shader.id(), camera, false, fogE);
            
            // Render Target Block Outline
            if (Config::currentState == GameState::PLAYING && !playerInventory.isVisible) {
                glm::ivec3 targetPos, tmpPos;
                float reach = (Config::currentMode == GameMode::SURVIVAL) ? 5.0f : 8.0f;
                if (raycast(camera.position(), camera.front(), reach, targetPos, tmpPos)) {
                    solidShader.use();
                    solidShader.setMat4("view", camera.viewMatrix());
                    solidShader.setMat4("projection", camera.projectionMatrix());
                    
                    glm::mat4 outlineModel = glm::mat4(1.0f);
                    outlineModel = glm::translate(outlineModel, glm::vec3(targetPos));
                    outlineModel = glm::translate(outlineModel, glm::vec3(-0.005f));
                    outlineModel = glm::scale(outlineModel, glm::vec3(1.01f));
                    
                    solidShader.setMat4("model", outlineModel);
                    solidShader.setVec4("color", glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
                    
                    glDisable(GL_DEPTH_TEST);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glLineWidth(2.5f);
                    glBindVertexArray(outlineVao);
                    glDrawArrays(GL_LINES, 0, 24);
                    glBindVertexArray(0);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    glEnable(GL_DEPTH_TEST);
                }
            }
            // Render Player (hands/legs always visible; head/body hidden in 1st person internally)
            {
                playerShader.use();
                playerShader.setMat4("view", camera.viewMatrix());
                playerShader.setMat4("projection", camera.projectionMatrix());
                playerShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
                playerShader.setInt("enableShaders", Config::enableShaders ? 1 : 0);
                playerShader.setInt("enableShadows", Config::enableShadows ? 1 : 0);
                playerShader.setVec3("lightColor", lightColor);
                playerShader.setVec3("skyColor", skyColor);
                playerShader.setVec3("lightDir", shaderLightDir);
                playerShader.setFloat("shadowIntensity", Config::shadowIntensity);
                
                playerShader.setInt("playerTexture", 0);
                playerShader.setInt("shadowMap", 1);
                
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, depthMap);
                static glm::vec3 lastPlayerPos = camera.position();
                glm::vec3 actualVelocity = (camera.position() - lastPlayerPos) / deltaTime;
                lastPlayerPos = camera.position();
                
                playerRenderer.render(camera, timeVal, actualVelocity, false, playerShader.id());
            }

            // Cloud Rendering Pass
            if (Config::enableClouds && Config::enableShaders) {
                glDepthMask(GL_FALSE); // Don't write to depth buffer
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                cloudShader.use();
                glm::mat4 invViewProj = glm::inverse(camera.projectionMatrix() * camera.viewMatrix());
                cloudShader.setMat4("invViewProj", invViewProj);
                cloudShader.setVec3("cameraPos", camera.position());
                cloudShader.setVec3("lightDir", shaderLightDir);
                cloudShader.setVec3("lightColor", lightColor);
                cloudShader.setVec3("skyColor", skyColor);
                cloudShader.setFloat("time", timeVal);
                cloudShader.setFloat("cloudHeight", Config::cloudHeight);
                cloudShader.setFloat("cloudScale", Config::cloudScale);
                cloudShader.setFloat("cloudSpeed", Config::cloudSpeed);
                cloudShader.setFloat("cloudDensity", Config::cloudDensity);
                cloudShader.setFloat("cloudThickness", Config::cloudThickness);
                cloudShader.setInt("cloudSteps", Config::cloudQuality);

                cloudShader.setFloat("sunSize", Config::sunSize);
                cloudShader.setFloat("sunIntensity", Config::sunIntensity);
                cloudShader.setVec3("sunColor", Config::sunColor);
                cloudShader.setFloat("moonSize", Config::moonSize);
                cloudShader.setFloat("moonIntensity", Config::moonIntensity);
                cloudShader.setFloat("starDensity", Config::starDensity);
                cloudShader.setFloat("milkyWayIntensity", Config::milkyWayIntensity);
                
                glBindVertexArray(cloudVAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);
                
                glDepthMask(GL_TRUE);
            }
        } else {
            // Main Menu / Background Canvas (Dirt Brown color)
            glClearColor(0.12f, 0.09f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // Draw Player Live UI Overlay inside playing mode loop
        if (Config::currentState == GameState::PLAYING) {
            
            // FPS Counter: updates once per second for readability
            static float fpsTimer = 0.0f;
            static float displayFps = 0.0f;
            static float avgFps = 0.0f;
            static int frameCount = 0;
            static float fpsAccum = 0.0f;
            
            frameCount++;
            fpsAccum += 1.0f / deltaTime;
            fpsTimer += deltaTime;
            if (fpsTimer >= 1.0f) {
                displayFps = 1.0f / deltaTime;
                avgFps = fpsAccum / frameCount;
                fpsTimer = 0.0f;
                frameCount = 0;
                fpsAccum = 0.0f;
            }
            
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.4f);
            ImGui::Begin("Debug_Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "FPS: %.0f", displayFps);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Avg: %.0f", avgFps);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "XYZ: %.1f / %.1f / %.1f", 
                camera.position().x, camera.position().y, camera.position().z);
            ImGui::End();

            // perfectly centered mathematical Crosshair
            ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();
            ImVec2 center(Config::windowWidth * 0.5f, Config::windowHeight * 0.5f);
            float crosshairSize = 10.0f;
            bgDrawList->AddLine(ImVec2(center.x - crosshairSize, center.y), ImVec2(center.x + crosshairSize, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
            bgDrawList->AddLine(ImVec2(center.x, center.y - crosshairSize), ImVec2(center.x, center.y + crosshairSize), IM_COL32(255, 255, 255, 200), 2.0f);

            // Custom Texture-based Hotbar Rendering
            float hotbarW = 182.0f * 2.5f; 
            float hotbarH = 22.0f * 2.5f;
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - hotbarW / 2.0f, Config::windowHeight - hotbarH - 20.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(hotbarW, hotbarH), ImGuiCond_Always);
            ImGui::Begin("Hotbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            
            unsigned int hotbarTex = TextureAtlas::getGuiTexture("hotbar");
            unsigned int selectTex = TextureAtlas::getGuiTexture("hotbar_selection");
            
            if (hotbarTex) {
                ImGui::GetWindowDrawList()->AddImage((void*)(uintptr_t)hotbarTex, ImGui::GetWindowPos(), 
                    ImVec2(ImGui::GetWindowPos().x + hotbarW, ImGui::GetWindowPos().y + hotbarH));
            }

            for (int i = 0; i < 9; ++i) {
                float slotX = ImGui::GetWindowPos().x + (i * 20.0f * 2.5f) + 1 * 2.5f;
                float slotY = ImGui::GetWindowPos().y + 1 * 2.5f;
                float slotSize = 20.0f * 2.5f;

                if (playerInventory.selectedHotbarIndex == i && selectTex) {
                    ImGui::GetWindowDrawList()->AddImage((void*)(uintptr_t)selectTex, ImVec2(slotX - 2 * 2.5f, slotY - 2 * 2.5f), 
                        ImVec2(slotX + slotSize + 2 * 2.5f, slotY + slotSize + 2 * 2.5f));
                }

                uint8_t item = playerInventory.slots[i].itemID;
                if (item > 1) {
                    // Draw block icon (using atlas UV)
                    float uvIdx = TextureAtlas::getUVForBlock(item, 4); // Side face icon
                    float atlasId = TextureAtlas::getTextureID();
                    // Since it's an array, ImGui can't easily draw it without a custom shader, 
                    // but we can use a placeholder for now or just text.
                    ImGui::SetCursorScreenPos(ImVec2(slotX + 5, slotY + 5));
                    ImGui::Text("%d", item);
                }
            }
            ImGui::End();

            // Custom Inventory Modal
            if (playerInventory.isVisible) {
                ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 250, Config::windowHeight / 2.0f - 150), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Always);
                ImGui::Begin("Inventory", &playerInventory.isVisible, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
                
                ImGui::Text("Creative Inventory");
                ImGui::Separator();
                
                for (int slot = 0; slot < 36; ++slot) {
                    if (slot % 9 != 0) ImGui::SameLine();
                    
                    uint8_t item = playerInventory.slots[slot].itemID;
                    std::string lbl = std::to_string(item) + "##inv" + std::to_string(slot);
                    if (item == 0) lbl = " ##inv" + std::to_string(slot);
                    
                    if (ImGui::Button(lbl.c_str(), ImVec2(48, 48))) {
                        if (slot < 9) playerInventory.setSelectedHotbar(slot);
                    }
                }
                
                ImGui::End();
                
                if (!playerInventory.isVisible) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true;
                }
            }
        }

        // GUI Rendering
        if (Config::currentState == GameState::MAIN_MENU) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 200, Config::windowHeight / 2.0f - 180), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400, 320), ImGuiCond_Always);
            ImGui::Begin("Voxel Game 3D", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
            
            ImGui::SetCursorPosY(20);
            ImGui::SetWindowFontScale(1.5f);
            float titleWidth = ImGui::CalcTextSize("ANTIGRAVITY VOXEL").x;
            ImGui::SetCursorPosX((400 - titleWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "ANTIGRAVITY VOXEL");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
            
            float btnX = (400 - 320) * 0.5f;
            ImGui::SetCursorPosX(btnX);
            if (ImGui::Button("Singleplayer", ImVec2(320, 50))) {
                Config::currentState = GameState::WORLD_SELECT;
            }
            ImGui::Spacing();
            ImGui::SetCursorPosX(btnX);
            if (ImGui::Button("Settings", ImVec2(320, 50))) {
                Config::currentState = GameState::SETTINGS;
            }
            ImGui::Spacing(); ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing(); ImGui::Spacing();
            ImGui::SetCursorPosX(btnX);
            if (ImGui::Button("Quit Game", ImVec2(320, 50))) {
                Config::save();
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::End();
        } 
        else if (Config::currentState == GameState::WORLD_SELECT) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 350, Config::windowHeight / 2.0f - 250), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_Always);
            ImGui::Begin("Select World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
            
            ImGui::SetCursorPosY(10);
            ImGui::SetWindowFontScale(1.2f);
            float selTitleWidth = ImGui::CalcTextSize("SELECT WORLD").x;
            ImGui::SetCursorPosX((700 - selTitleWidth) * 0.5f);
            ImGui::Text("SELECT WORLD");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            
            auto worlds = WorldManager::getSavedWorlds();
            
            ImGui::BeginChild("WorldList", ImVec2(0, 360), true);
            if (worlds.empty()) {
                ImGui::SetCursorPosY(150);
                float emptyW = ImGui::CalcTextSize("No worlds found. Create a new one!").x;
                ImGui::SetCursorPosX((680 - emptyW) * 0.5f);
                ImGui::TextDisabled("No worlds found. Create a new one!");
            } else {
                for (const auto& w : worlds) {
                    ImGui::PushID(w.folderName.c_str());
                    std::string treeLabel = w.displayName + "##" + w.folderName;
                    
                    if (ImGui::TreeNodeEx(treeLabel.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                        std::string modeStr = (w.mode == GameMode::SURVIVAL) ? "Survival" : ((w.mode == GameMode::CREATIVE) ? "Creative" : "Spectator");
                        std::string typeStr = (w.worldType == 1) ? "Flat" : ((w.worldType == 3) ? "V2" : "V1");
                        int mins = w.timePlayedSeconds / 60;
                        int hrs = mins / 60; mins = mins % 60;

                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Mode: %-10s | Type: %-4s | Time Played: %dh %dm", modeStr.c_str(), typeStr.c_str(), hrs, mins);
                        ImGui::SameLine(ImGui::GetWindowWidth() - 140);
                        
                        if (ImGui::Button("PLAY WORLD", ImVec2(120, 30))) {
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
                    ImGui::PopID();
                    ImGui::Spacing();
                }
            }
            ImGui::EndChild();
            
            ImGui::Spacing();
            ImGui::SetCursorPosX(110);
            if (ImGui::Button("Create New World", ImVec2(220, 45))) {
                // Determine next "New World N" name
                int nextId = 1;
                bool nameExists = true;
                std::string propName;
                while (nameExists) {
                    propName = "New World " + std::to_string(nextId);
                    nameExists = false;
                    for (const auto& w : worlds) {
                        if (w.displayName == propName || w.folderName == propName) {
                            nameExists = true; break;
                        }
                    }
                    if (nameExists) nextId++;
                }
                snprintf(createWorldNameBuf, sizeof(createWorldNameBuf), "%s", propName.c_str());
                Config::currentState = GameState::CREATE_WORLD;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(370);
            if (ImGui::Button("Back to Menu", ImVec2(220, 45))) {
                Config::currentState = GameState::MAIN_MENU;
            }
            ImGui::End();
        }
        else if (Config::currentState == GameState::CREATE_WORLD) {
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 250, Config::windowHeight / 2.0f - 200), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Always);
            ImGui::Begin("Create New World", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            ImGui::Spacing();
            ImGui::InputText("World Name", createWorldNameBuf, IM_ARRAYSIZE(createWorldNameBuf));
            
            static char seedBuf[64] = "";
            ImGui::InputText("Seed (Leave empty for random)", seedBuf, IM_ARRAYSIZE(seedBuf));
            
            static int selectedMode = 1; // 0: Survival, 1: Creative, 2: Spectator
            ImGui::Combo("Game Mode", &selectedMode, "Survival\0Creative\0Spectator\0");
            
            static int selectedTerrain = 3; // 0: Default(V1), 1: Flat, 2: Skyblock, 3: V2
            ImGui::Combo("Terrain Type", &selectedTerrain, "Default (V1)\0Flat World\0Skyblock\0WorldGen V2\0");
            
            ImGui::Separator();
            
            if (ImGui::Button("Create & Play!", ImVec2(380, 40))) {
                auto newMode = (selectedMode == 0) ? GameMode::SURVIVAL : ((selectedMode == 1) ? GameMode::CREATIVE : GameMode::SPECTATOR);
                
                int parsedSeed;
                std::string seedStr(seedBuf);
                if (seedStr.empty()) {
                    parsedSeed = static_cast<int>(getCurrentTimeMs() & 0x7FFFFFFF); // Random positive int based on time
                } else {
                    try {
                        parsedSeed = std::stoi(seedStr);
                    } catch (...) {
                        parsedSeed = static_cast<int>(std::hash<std::string>{}(seedStr) & 0x7FFFFFFF);
                    }
                }
                
                WorldMetadata newMeta;
                newMeta.displayName = std::string(createWorldNameBuf);
                if (newMeta.displayName.empty()) newMeta.displayName = "Unnamed_World";
                
                // Keep file names path safe
                newMeta.folderName = newMeta.displayName; 
                std::replace(newMeta.folderName.begin(), newMeta.folderName.end(), ' ', '_'); 
                newMeta.folderName += "_" + std::to_string(std::abs(parsedSeed));
                
                newMeta.seed = parsedSeed;
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
            ImGui::SetNextWindowPos(ImVec2(Config::windowWidth / 2.0f - 300, 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(600, Config::windowHeight - 80.0f), ImGuiCond_Always);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            if (ImGui::BeginTabBar("SettingsTabs")) {
                if (ImGui::BeginTabItem("Video & Quality")) {
                    ImGui::Spacing();
                    
                    const char* presets[] = { "Low", "Medium", "High", "Ultra", "Custom" };
                    int currentPreset = static_cast<int>(Config::graphicsPreset);
                    if (ImGui::Combo("Graphics Quality Preset", &currentPreset, presets, 5)) {
                        applyGraphicsPreset(static_cast<GraphicsPreset>(currentPreset));
                    }
                    
                    if (Config::graphicsPreset == GraphicsPreset::ULTRA) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "  All visual features enabled at maximum quality (Photon Mode)");
                    }
                    
                    ImGui::Spacing();
                    if (ImGui::CollapsingHeader("Display & Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::Checkbox("Fullscreen Mode", &Config::isFullscreen)) {
                            GLFWmonitor* primary = glfwGetPrimaryMonitor();
                            const GLFWvidmode* mod = glfwGetVideoMode(primary);
                            if (Config::isFullscreen) {
                                glfwSetWindowMonitor(window, primary, 0, 0, mod->width, mod->height, mod->refreshRate);
                            } else {
                                glfwSetWindowMonitor(window, nullptr, 100, 100, 1280, 720, 0);
                            }
                        }
                        ImGui::SliderInt("Render Distance", &Config::renderDistance, 4, 48);
                        ImGui::SliderInt("Vertical Distance", &Config::renderDistanceY, 2, 10);
                        if (ImGui::SliderFloat("Field of View", &Config::cameraFov, 60.0f, 120.0f)) {
                            camera.setFov(Config::cameraFov);
                        }
                    }

                    if (ImGui::CollapsingHeader("Performance & LOD")) {
                        ImGui::SliderInt("Max FPS (0=Unlimited)", &Config::fpsCap, 0, 500);
                        ImGui::Checkbox("VSync", &Config::vsync);
                        ImGui::Separator();
                        ImGui::Checkbox("Enable LOD (Distant Horizons)", &Config::enableLOD);
                        if (Config::enableLOD) {
                            ImGui::SliderInt("LOD View Distance", &Config::lodDistance, 8, 512);
                            ImGui::SliderInt("LOD Quality (Lower is better)", &Config::lodQuality, 1, 8);
                        }
                    }
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Shaders & Environment")) {
                    ImGui::Spacing();
                    ImGui::Checkbox("Enable Advanced Shaders", &Config::enableShaders);
                    
                    if (Config::enableShaders) {
                        if (ImGui::CollapsingHeader("Atmosphere (Sun/Moon/Stars)")) {
                            ImGui::SliderFloat("Sun Size", &Config::sunSize, 0.005f, 0.04f, "%.3f");
                            ImGui::ColorEdit3("Sun Color", &Config::sunColor.x);
                            ImGui::SliderFloat("Sun Intensity", &Config::sunIntensity, 0.5f, 2.5f);
                            ImGui::Separator();
                            ImGui::SliderFloat("Moon Size", &Config::moonSize, 0.005f, 0.03f, "%.3f");
                            ImGui::SliderFloat("Moon Intensity", &Config::moonIntensity, 0.0f, 1.5f);
                            ImGui::Separator();
                            ImGui::SliderFloat("Star Density", &Config::starDensity, 0.0f, 5.0f);
                            ImGui::SliderFloat("Milky Way Intensity", &Config::milkyWayIntensity, 0.0f, 1.0f);
                            ImGui::Separator();
                            ImGui::SliderFloat("God Rays", &Config::godRaysIntensity, 0.0f, 1.0f);
                        }

                        if (ImGui::CollapsingHeader("Water Effects")) {
                            const char* waterModes[] = { "Basic (Flat)", "Advanced (Photon Waves)" };
                            ImGui::Combo("Rendering Mode", &Config::waterMode, waterModes, 2);
                            if (Config::waterMode == 1) {
                                ImGui::SliderFloat("Transparency", &Config::waterTransparency, 0.1f, 1.0f);
                                ImGui::SliderFloat("Wave Speed", &Config::waterWaveSpeed, 0.1f, 3.0f);
                                ImGui::SliderFloat("Wave Height", &Config::waterWaveHeight, 0.0f, 1.0f);
                                ImGui::Checkbox("Enable Real-time Reflections", &Config::enableWaterReflections);
                            }
                        }

                        if (ImGui::CollapsingHeader("Shadows")) {
                            ImGui::Checkbox("Enable Active Shadows", &Config::enableShadows);
                            if (Config::enableShadows) {
                                ImGui::SliderFloat("Intensity", &Config::shadowIntensity, 0.1f, 1.0f);
                                ImGui::SliderFloat("Softness (PCSS)", &Config::shadowSoftness, 0.1f, 2.0f);
                                ImGui::SliderFloat("Bias", &Config::shadowBias, 0.001f, 0.02f, "%.4f");
                                const char* shadowSizes[] = { "512", "1024", "2048", "4096" };
                                int currentSizeIdx = 0;
                                if (Config::shadowMapSize <= 512) currentSizeIdx = 0;
                                else if (Config::shadowMapSize <= 1024) currentSizeIdx = 1;
                                else if (Config::shadowMapSize <= 2048) currentSizeIdx = 2;
                                else currentSizeIdx = 3;
                                
                                if (ImGui::Combo("Resolution", &currentSizeIdx, shadowSizes, 4)) {
                                    if (currentSizeIdx == 0) Config::shadowMapSize = 512;
                                    else if (currentSizeIdx == 1) Config::shadowMapSize = 1024;
                                    else if (currentSizeIdx == 2) Config::shadowMapSize = 2048;
                                    else Config::shadowMapSize = 4096;
                                }
                            }
                        }

                        if (ImGui::CollapsingHeader("Lighting & Fog")) {
                            ImGui::Checkbox("Enable Fog", &Config::enableFog);
                            if (Config::enableFog) {
                                ImGui::SliderFloat("Fog Density", &Config::fogDensity, 0.0001f, 0.05f, "%.4f");
                                ImGui::ColorEdit3("Fog Color", &Config::fogColor.x);
                                ImGui::SliderFloat("Height Falloff", &Config::fogHeightFalloff, 0.001f, 0.1f);
                            }
                            ImGui::Separator();
                            ImGui::Checkbox("Directional Shading", &Config::enableDirectionalFaceShading);
                            ImGui::SliderFloat("Ambient Light", &Config::ambientBrightness, 0.0f, 1.0f);
                            ImGui::SliderFloat("Day/Night Speed", &Config::dayTimeSpeed, 0.0f, 0.02f, "%.4f");
                        }

                        if (ImGui::CollapsingHeader("Post-Processing")) {
                            ImGui::SliderFloat("Saturation", &Config::saturation, 0.0f, 2.0f);
                            ImGui::SliderFloat("Contrast", &Config::contrast, 0.5f, 1.5f);
                            ImGui::Checkbox("Wind Effects (Leaves)", &Config::enableLeafWind);
                        }
                        
                        if (ImGui::CollapsingHeader("Clouds Rendering")) {
                            ImGui::Checkbox("Enable Volumetric Clouds", &Config::enableClouds);
                            if (Config::enableClouds) {
                                ImGui::SliderFloat("Height", &Config::cloudHeight, 100.0f, 800.0f);
                                ImGui::SliderFloat("Thickness", &Config::cloudThickness, 20.0f, 400.0f);
                                ImGui::SliderFloat("Density", &Config::cloudDensity, 0.1f, 1.0f);
                                ImGui::SliderFloat("Scale", &Config::cloudScale, 0.0001f, 0.001f, "%.4f");
                                ImGui::SliderFloat("Speed", &Config::cloudSpeed, 0.001f, 0.15f, "%.3f");
                                ImGui::SliderInt("Quality (Raysteps)", &Config::cloudQuality, 4, 64);
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::Spacing();
                    ImGui::SeparatorText("Movement");
                    ImGui::SliderFloat("Player Speed", &Config::playerSpeed, 2.0f, 20.0f);
                    ImGui::SliderFloat("Sprint Speed", &Config::playerSprintSpeed, 5.0f, 50.0f);
                    ImGui::SliderFloat("Mouse Sensitivity", &Config::mouseSensitivity, 0.05f, 0.5f);
                    
                    ImGui::SeparatorText("Zoom");
                    ImGui::SliderFloat("Zoom FOV", &Config::zoomBaseFov, 15.0f, 60.0f);
                    
                    ImGui::SeparatorText("Keybinds");
                    static std::string waitingForKey = "";
                    auto keybindRow = [&](const char* label, const std::string& action) {
                        ImGui::Text("%s", label);
                        ImGui::SameLine(200);
                        std::string btnLabel = (waitingForKey == action) ? "[ Press a key... ]" : InputManager::getKeyName(action);
                        if (ImGui::Button((btnLabel + "##" + action).c_str(), ImVec2(150, 0))) waitingForKey = action;
                    };
                    
                    keybindRow("Move Forward", InputAction::MOVE_FORWARD);
                    keybindRow("Move Backward", InputAction::MOVE_BACK);
                    keybindRow("Move Left", InputAction::MOVE_LEFT);
                    keybindRow("Move Right", InputAction::MOVE_RIGHT);
                    keybindRow("Jump / Fly Up", InputAction::JUMP);
                    keybindRow("Descend", InputAction::DESCEND);
                    keybindRow("Sprint", InputAction::SPRINT);
                    keybindRow("Inventory", InputAction::INVENTORY);
                    
                    if (!waitingForKey.empty()) {
                        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++) {
                            if (glfwGetKey(window, key) == GLFW_PRESS) {
                                InputManager::setKey(waitingForKey, key);
                                waitingForKey = "";
                                break;
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            if (ImGui::Button("Save & Back to Game", ImVec2(585, 40))) {
                Config::save();
                // If we have a folder name, we are actually in a world!
                if (!WorldManager::currentWorld.folderName.empty()) Config::currentState = GameState::PAUSED;
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
                WorldManager::currentWorld = WorldMetadata(); // Reset current world metadata
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
        // Apply VSync setting
        glfwSwapInterval(Config::vsync ? 1 : 0);
        
        if (!Config::vsync && Config::fpsCap > 0) {
            float frameTime = static_cast<float>(glfwGetTime()) - currentFrame;
            float frameTarget = 1.0f / static_cast<float>(Config::fpsCap);
            if (frameTime < frameTarget) {
                while (static_cast<float>(glfwGetTime()) - currentFrame < frameTarget) {}
            }
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
