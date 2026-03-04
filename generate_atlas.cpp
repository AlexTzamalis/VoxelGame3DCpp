#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vector>
#include <iostream>

void drawTile(std::vector<unsigned char>& img, int atlasW, int ux, int uy, int tileSize, int r, int g, int b) {
    for (int y = uy * tileSize; y < (uy+1)*tileSize; ++y) {
        for (int x = ux * tileSize; x < (ux+1)*tileSize; ++x) {
            int idx = (y * atlasW + x) * 4;
            // Draw a slightly darker border around each tile
            if (x == ux*tileSize || y == uy*tileSize || x == (ux+1)*tileSize-1 || y == (uy+1)*tileSize-1) {
                img[idx] = r*0.7; img[idx+1] = g*0.7; img[idx+2] = b*0.7; img[idx+3] = 255;
            } else {
                img[idx] = r; img[idx+1] = g; img[idx+2] = b; img[idx+3] = 255;
            }
        }
    }
}

int main() {
    int tileSize = 64; 
    int atlasW = tileSize * 16;
    int atlasH = tileSize * 16;
    std::vector<unsigned char> img(atlasW * atlasH * 4, 0); // All transparent

    // Fill each tile with distinct bright colors so debugging is easy!
    for (int ty=0; ty<16; ++ty) {
        for (int tx=0; tx<16; ++tx) {
            if (tx==0 && ty==15) continue; // Let's say top left usually is empty?
            if (tx==0 && ty==0) continue; // bottom left
            int r = (tx * 16) % 256;
            int g = (ty * 16) % 256;
            int b = 150;
            
            // Explicit colors for typical blocks used in Chunk.cpp:
            // Dirt: type 1 -> tx=1, ty=15
            if (tx == 1 && ty == 15) { r = 139; g = 69; b = 19; } // Brown
            // Stone: type 2 -> tx=2, ty=15
            if (tx == 2 && ty == 15) { r = 120; g = 120; b = 120; } // Grey

            // If OpenGL UVs are inverted (top vs bottom row)
            // Dirt inverted -> tx=1, ty=0
            if (tx == 1 && ty == 0) { r = 160; g = 82; b = 45; } // Sienna
            // Stone inverted -> tx=2, ty=0
            if (tx == 2 && ty == 0) { r = 169; g = 169; b = 169; } // Dark grey
            
            drawTile(img, atlasW, tx, ty, tileSize, r, g, b);
        }
    }

    if (stbi_write_png("assets/textures-atlas.png", atlasW, atlasH, 4, img.data(), atlasW*4)) {
        std::cout << "Successfully generated texture atlas!\n";
    } else {
        std::cout << "Failed\n";
    }
    return 0;
}
