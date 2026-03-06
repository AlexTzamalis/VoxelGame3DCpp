#pragma once

#include <string>

class Texture {
public:
    Texture() = default;
    ~Texture();

    bool loadFromFile(const std::string& path);
    void bind(unsigned int unit = 0) const;
    void unbind() const;

    unsigned int id() const { return textureId_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    unsigned int textureId_ = 0;
    int width_ = 0;
    int height_ = 0;
};
