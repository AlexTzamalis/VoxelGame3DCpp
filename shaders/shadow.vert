#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in uint aData;
layout (location = 2) in vec4 aColor;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform float time;

out vec3 TexCoord;
out vec4 VertexColor;

void main() {
    uint width = (aData >> 3u) & 31u;
    uint height = (aData >> 8u) & 31u;
    uint layer = (aData >> 13u) & 65535u;
    uint corner = (aData >> 29u) & 3u;
    
    float u = (corner == 2u || corner == 3u) ? float(width) : 0.0;
    float v = (corner == 0u || corner == 3u) ? float(height) : 0.0;
    vec3 aTexCoord = vec3(u, v, float(layer));

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
