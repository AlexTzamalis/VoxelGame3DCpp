#include "Config.hpp"

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
}
