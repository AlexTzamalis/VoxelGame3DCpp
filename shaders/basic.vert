#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTexCoord;
layout (location = 3) in vec4 aColor;

out vec3 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 VertexColor;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float time;
uniform int enableShaders;

void main() {
    vec3 wPos = vec3(model * vec4(aPos, 1.0));
    
    if (enableShaders == 1) {
        if (aColor.a > 0.65 && aColor.a < 0.75) {
            // Water wave effect (animate top faces)
            if (aNormal.y > 0.5) {
                wPos.y += sin(wPos.x * 2.5 + time * 0.8) * 0.05 + cos(wPos.z * 2.0 + time * 0.6) * 0.05;
            }
        } else if (aColor.a > 0.8 && aColor.a < 0.9) {
            // Leaves wind effect
            wPos.x += sin(wPos.y * 2.0 + time * 0.6) * 0.03;
            wPos.z += cos(wPos.y * 2.0 + time * 0.6) * 0.03;
        }
    }

    FragPos = wPos;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    VertexColor = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
