#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Config {
    // Defaults that we can later load from a settings.json file
    int windowWidth = 1280;
    int windowHeight = 720;
    int fpsCap = 144; // Can be used later to throttle glfwSwapInterval or thread sleeping if needed

    float playerSpeed = 10.0f; // 5
    float playerSprintSpeed = 15.0f; // 10
    float mouseSensitivity = 0.1f;
    float cameraFov = 75.0f; // Note: You'll want to update Camera.hpp to use this eventually

    int renderDistance = 12; // Adjusted down for better framerates on lower-end Systems
    int renderDistanceY = 10; // Massively expanded max chunks rendered per column
    bool frustumCulling = true; // Future toggle
    float fogDensity = 0.005f; // Exponential fog density
    bool enableShaders = true;
    bool enableShadows = true;
    float dayTimeSpeed = 0.0021816f; // Exactly 48 minutes per 24h cycle
    
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
                else if (key == "ENABLE_SHADERS") file >> enableShaders;
                else if (key == "ENABLE_SHADOWS") file >> enableShadows;
                else if (key == "DAY_SPEED") file >> dayTimeSpeed;
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
                 << "ENABLE_SHADERS " << enableShaders << "\n"
                 << "ENABLE_SHADOWS " << enableShadows << "\n"
                 << "DAY_SPEED " << dayTimeSpeed << "\n"
                 << "PLAYER_SPEED " << playerSpeed << "\n"
                 << "SPRINT_SPEED " << playerSprintSpeed << "\n"
                 << "SENSITIVITY " << mouseSensitivity << "\n";
        }
    }
}
