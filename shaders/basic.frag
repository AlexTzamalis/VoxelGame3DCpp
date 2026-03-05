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
    vec3 N = normalize(Normal);
    
    // Smooth, realistic voxel ambient lighting setup based on face direction
    float skyLight = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
    vec3 ambient = mix(vec3(0.35, 0.45, 0.6), vec3(1.0, 0.95, 0.9), skyLight) * 0.7;
    
    // Explicit fixed directional sun
    float diff = max(dot(N, normalize(-lightDir)), 0.0);
    vec3 diffuse = diff * lightColor * 0.8;
    
    vec3 result = (ambient + diffuse) * texColor.rgb;
    
    // Calculate distance from fragment to camera in world space
    float distance = length(FragPos - cameraPos);
    
    // Smooth fog for softer atmospheric blending
    float fogFactor = smoothstep(fogStart, fogEnd, distance);
    
    // Blend final pixel color into the skybox based on distance
    FragColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
}
