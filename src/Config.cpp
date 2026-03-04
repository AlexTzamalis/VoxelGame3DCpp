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

    int renderDistance = 8; // Radiates out in a grid. E.g. Distance 4 = 9x9 grid = 81 Chunks.
    bool frustumCulling = true; // Future toggle
    float fogDensity = 0.015f; // Exponential fog density
}
