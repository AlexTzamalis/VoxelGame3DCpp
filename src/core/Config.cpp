#include "core/Config.hpp"
#include "game/InputManager.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Config {
    int windowWidth = 1280;
    int windowHeight = 720;
    bool isFullscreen = true;
    int fpsCap = 240;
    bool vsync = false;

    float playerSpeed = 4.317f;
    float playerSprintSpeed = 5.6f;
    float playerSneakSpeed = 1.3f;
    float mouseSensitivity = 0.1f;
    float cameraFov = 75.0f;
    float zoomBaseFov = 30.0f;

    int renderDistance = 12;
    int renderDistanceY = 4;
    bool frustumCulling = true;

    bool enableLOD = true;
    int lodDistance = 256; // 4km horizon
    int lodQuality = 2;   // High detail distant terrain (8x8 grid)
    
    // Master shader toggle
    bool enableShaders = true;
    
    // Shadows
    bool enableShadows = true;
    float shadowIntensity = 0.45f;    // Gentle gray, not pitch black
    int shadowMapSize = 2048;
    
    // Water
    int waterMode = 1;                // 0=Basic, 1=Advanced
    
    // Lighting
    bool enableFog = true;
    float fogDensity = 0.005f;
    bool enableDirectionalFaceShading = true;
    float ambientBrightness = 0.35f;  // Brighter, richer environment lighting
    float dayTimeSpeed = 0.0021816f;
    
    // Wind
    bool enableLeafWind = true;
    
    // Ultra mode
    bool ultraMode = false;
    
    // Clouds
    bool enableClouds = true;
    float cloudHeight = 150.0f;
    float cloudScale = 0.0008f;
    float cloudSpeed = 0.015f;  // Photon-like slow drift
    float saturation = 0.85f;  // Toned down for realism
    float contrast = 1.05f;    // Slightly more punch
    
    GameMode currentMode = GameMode::CREATIVE;
    GameState currentState = GameState::MAIN_MENU;
    int currentWorldType = 0;
    int currentSeed = 1337;

    void loadKeybinds() {
        std::ifstream file("keybinds.ini");
        if (file.is_open()) {
            std::string action;
            int key;
            while (file >> action >> key) {
                InputManager::setKey(action, key);
            }
        }
    }

    void saveKeybinds() {
        std::ofstream file("keybinds.ini");
        if (file.is_open()) {
            for (const auto& [action, key] : InputManager::getAllBindings()) {
                file << action << " " << key << "\n";
            }
        }
    }

    void load() {
        std::ifstream file("settings.ini");
        if (file.is_open()) {
            std::string key;
            while(file >> key) {
                if (key == "RENDER_DISTANCE") file >> renderDistance;
                else if (key == "RENDER_DISTANCE_Y") file >> renderDistanceY;
                else if (key == "ENABLE_LOD") file >> enableLOD;
                else if (key == "LOD_DISTANCE") file >> lodDistance;
                else if (key == "LOD_QUALITY") file >> lodQuality;
                else if (key == "FOV") file >> cameraFov;
                else if (key == "MAX_FPS") file >> fpsCap;
                else if (key == "VSYNC") file >> vsync;
                else if (key == "IS_FULLSCREEN") file >> isFullscreen;
                else if (key == "ENABLE_SHADERS") file >> enableShaders;
                else if (key == "ENABLE_SHADOWS") file >> enableShadows;
                else if (key == "SHADOW_INTENSITY") file >> shadowIntensity;
                else if (key == "SHADOW_MAP_SIZE") file >> shadowMapSize;
                else if (key == "WATER_MODE") file >> waterMode;
                else if (key == "ENABLE_FOG") file >> enableFog;
                else if (key == "FACE_SHADING") file >> enableDirectionalFaceShading;
                else if (key == "AMBIENT_BRIGHTNESS") file >> ambientBrightness;
                else if (key == "DAY_SPEED") file >> dayTimeSpeed;
                else if (key == "LEAF_WIND") file >> enableLeafWind;
                else if (key == "ULTRA_MODE") file >> ultraMode;
                else if (key == "ENABLE_CLOUDS") file >> enableClouds;
                else if (key == "CLOUD_HEIGHT") file >> cloudHeight;
                else if (key == "CLOUD_SCALE") file >> cloudScale;
                else if (key == "CLOUD_SPEED") file >> cloudSpeed;
                else if (key == "SATURATION") file >> saturation;
                else if (key == "CONTRAST") file >> contrast;
                else if (key == "PLAYER_SPEED") file >> playerSpeed;
                else if (key == "SPRINT_SPEED") file >> playerSprintSpeed;
                else if (key == "SENSITIVITY") file >> mouseSensitivity;
                else if (key == "ZOOM_FOV") file >> zoomBaseFov;
            }
        }
        loadKeybinds();
    }

    void save() {
        std::ofstream file("settings.ini");
        if (file.is_open()) {
            file << "RENDER_DISTANCE " << renderDistance << "\n"
                 << "RENDER_DISTANCE_Y " << renderDistanceY << "\n"
                 << "ENABLE_LOD " << enableLOD << "\n"
                 << "LOD_DISTANCE " << lodDistance << "\n"
                 << "LOD_QUALITY " << lodQuality << "\n"
                 << "FOV " << cameraFov << "\n"
                 << "MAX_FPS " << fpsCap << "\n"
                 << "VSYNC " << vsync << "\n"
                 << "IS_FULLSCREEN " << isFullscreen << "\n"
                 << "ENABLE_SHADERS " << enableShaders << "\n"
                 << "ENABLE_SHADOWS " << enableShadows << "\n"
                 << "SHADOW_INTENSITY " << shadowIntensity << "\n"
                 << "SHADOW_MAP_SIZE " << shadowMapSize << "\n"
                 << "WATER_MODE " << waterMode << "\n"
                 << "ENABLE_FOG " << enableFog << "\n"
                 << "FACE_SHADING " << enableDirectionalFaceShading << "\n"
                 << "AMBIENT_BRIGHTNESS " << ambientBrightness << "\n"
                 << "DAY_SPEED " << dayTimeSpeed << "\n"
                 << "LEAF_WIND " << enableLeafWind << "\n"
                 << "ULTRA_MODE " << ultraMode << "\n"
                 << "ENABLE_CLOUDS " << enableClouds << "\n"
                 << "CLOUD_HEIGHT " << cloudHeight << "\n"
                 << "CLOUD_SCALE " << cloudScale << "\n"
                 << "CLOUD_SPEED " << cloudSpeed << "\n"
                 << "SATURATION " << saturation << "\n"
                 << "CONTRAST " << contrast << "\n"
                 << "PLAYER_SPEED " << playerSpeed << "\n"
                 << "SPRINT_SPEED " << playerSprintSpeed << "\n"
                 << "SENSITIVITY " << mouseSensitivity << "\n"
                 << "ZOOM_FOV " << zoomBaseFov << "\n";
        }
        saveKeybinds();
    }
}
