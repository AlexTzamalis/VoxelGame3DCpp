#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertexColor;

out vec4 FragColor;

uniform sampler2D textureAtlas;
uniform vec3 lightDir;
uniform vec3 lightColor;

// Variables for Atmospheric Fog
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform float fogStart;
uniform float fogEnd;

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard; // Support true cut-out transparencies for leaves
    
    texColor *= VertexColor; // Applies RGB biome tint AND the alpha channel (0.7f for water)
    float diff = max(dot(normalize(Normal), normalize(-lightDir)), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 ambient = 0.3 * lightColor;
    
    vec3 result = (ambient + diffuse) * texColor.rgb;
    
    // Calculate distance from fragment to camera in world space
    float distance = length(FragPos - cameraPos);
    
    // Linear fog based on Render Distance
    float fogFactor = (distance - fogStart) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Blend final pixel color into the skybox based on distance
    FragColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
}
