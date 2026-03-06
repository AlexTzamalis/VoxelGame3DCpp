#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Config {
    int windowWidth = 1280;
    int windowHeight = 720;
    int fpsCap = 240;
    bool vsync = false;

    float playerSpeed = 10.0f;
    float playerSprintSpeed = 15.0f;
    float mouseSensitivity = 0.1f;
    float cameraFov = 75.0f;

    int renderDistance = 12;
    int renderDistanceY = 10;
    bool frustumCulling = true;
    
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
    float ambientBrightness = 0.25f;  // Brighter nights
    float dayTimeSpeed = 0.0021816f;
    
    // Wind
    bool enableLeafWind = true;
    
    GameMode currentMode = GameMode::CREATIVE;
    GameState currentState = GameState::MAIN_MENU;
    int currentWorldType = 0;
    int currentSeed = 1337;

    void load() {
        std::ifstream file("settings.ini");
        if (file.is_open()) {
            std::string key;
            while(file >> key) {
                if (key == "RENDER_DISTANCE") file >> renderDistance;
                else if (key == "RENDER_DISTANCE_Y") file >> renderDistanceY;
                else if (key == "FOV") file >> cameraFov;
                else if (key == "MAX_FPS") file >> fpsCap;
                else if (key == "VSYNC") file >> vsync;
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
                else if (key == "PLAYER_SPEED") file >> playerSpeed;
                else if (key == "SPRINT_SPEED") file >> playerSprintSpeed;
                else if (key == "SENSITIVITY") file >> mouseSensitivity;
            }
        }
    }

    void save() {
        std::ofstream file("settings.ini");
        if (file.is_open()) {
            file << "RENDER_DISTANCE " << renderDistance << "\n"
                 << "RENDER_DISTANCE_Y " << renderDistanceY << "\n"
                 << "FOV " << cameraFov << "\n"
                 << "MAX_FPS " << fpsCap << "\n"
                 << "VSYNC " << vsync << "\n"
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
                 << "PLAYER_SPEED " << playerSpeed << "\n"
                 << "SPRINT_SPEED " << playerSprintSpeed << "\n"
                 << "SENSITIVITY " << mouseSensitivity << "\n";
        }
    }
}
