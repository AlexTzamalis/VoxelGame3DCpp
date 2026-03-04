#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include <iostream>

int main() {
    int w, h, c;
    unsigned char *data = stbi_load("assets/textures-atlas.png", &w, &h, &c, 4);
    if (!data) return 1;
    bool hasColor = false;
    for (int i = 0; i < w * h * 4; i+=4) {
        if (data[i] > 0 || data[i+1] > 0 || data[i+2] > 0) {
            hasColor = true; break;
        }
    }
    std::cout << "Has Color? " << (hasColor ? "Yes" : "No") << "\n";
    return 0;
}
