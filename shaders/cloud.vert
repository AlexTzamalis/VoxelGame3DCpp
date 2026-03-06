#version 330 core
layout (location = 0) in vec2 aPos;

out vec2 ScreenUV;

void main() {
    ScreenUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.999, 1.0); // Behind everything but skybox
}
