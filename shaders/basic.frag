#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 VertexColor;

out vec4 FragColor;

uniform sampler2D textureAtlas;
uniform vec3 lightDir;
uniform vec3 lightColor;

// Variables for Atmospheric Fog
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform float fogDensity;

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    texColor.rgb *= VertexColor;
    float diff = max(dot(normalize(Normal), normalize(-lightDir)), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 ambient = 0.3 * lightColor;
    
    vec3 result = (ambient + diffuse) * texColor.rgb;
    
    // Calculate distance from fragment to camera in world space
    float distance = length(FragPos - cameraPos);
    
    // Exponential squared fog blending
    float fogFactor = exp(-pow(distance * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Blend final pixel color into the skybox based on distance
    FragColor = vec4(mix(skyColor, result, fogFactor), texColor.a);
}
