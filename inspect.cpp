#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include <iostream>

int main() {
    int w, h, c;
    unsigned char *data = stbi_load("assets/textures-atlas.png", &w, &h, &c, 4);
    if (!data) { std::cout << "Failed to load image.\n"; return 1; }
    std::cout << "Loaded " << w << "x" << h << "\n";
    int tileW = w / 16;
    int tileH = h / 16;
    for (int ty = 0; ty < 16; ++ty) {
        for (int tx = 0; tx < 16; ++tx) {
            bool hasData = false;
            for (int y = ty * tileH; y < (ty+1)*tileH; ++y) {
                for (int x = tx * tileW; x < (tx+1)*tileW; ++x) {
                    if (data[(y*w + x)*4 + 3] > 10) { hasData = true; break; }
                }
                if (hasData) break;
            }
            if (hasData) std::cout << "Tile (" << tx << "," << ty << ") has data.\n";
        }
    }
    return 0;
}
