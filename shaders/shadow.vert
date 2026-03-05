#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec3 aTexCoord;
layout (location = 3) in vec4 aColor;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform float time;

out vec3 TexCoord;
out vec4 VertexColor;

void main() {
    TexCoord = aTexCoord;
    VertexColor = aColor;
    
    vec3 wPos = vec3(model * vec4(aPos, 1.0));
    
    if (aColor.a > 0.65 && aColor.a < 0.75) {
        // Match water wave dynamics to project identical shadow footprint
        wPos.y += sin(wPos.x * 2.5 + time * 0.8) * 0.05 + cos(wPos.z * 2.0 + time * 0.6) * 0.05;
    } else if (aColor.a > 0.8 && aColor.a < 0.9) {
        // Leaves wind effect applied to shadows
        wPos.x += sin(wPos.y * 2.0 + time * 0.6) * 0.03;
        wPos.z += cos(wPos.y * 2.0 + time * 0.6) * 0.03;
    }
    
    gl_Position = lightSpaceMatrix * vec4(wPos, 1.0);
}
