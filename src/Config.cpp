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
    int renderDistanceY = 5; // Up/down chunk loading limit (Handles roughly Y: -30 to 130)
    bool frustumCulling = true; // Future toggle
    float fogDensity = 0.005f; // Exponential fog density
    
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
                 << "PLAYER_SPEED " << playerSpeed << "\n"
                 << "SPRINT_SPEED " << playerSprintSpeed << "\n"
                 << "SENSITIVITY " << mouseSensitivity << "\n";
        }
    }
}
