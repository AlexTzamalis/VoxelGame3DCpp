#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform sampler2D playerTexture;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform vec3 lightDir;
uniform int enableShaders;
uniform int enableShadows;
uniform sampler2D shadowMap;
uniform float shadowIntensity;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (enableShadows == 0) return 0.0;
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0) return 0.0;
    
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? shadowIntensity : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec4 texColor = texture(playerTexture, TexCoord);
    if(texColor.a < 0.1) discard;

    if (enableShaders == 1) {
        vec3 norm = normalize(Normal);
        vec3 lDir = normalize(lightDir);
        
        // Ambient
        vec3 ambient = skyColor * 0.4;
        
        // Diffuse
        float diff = max(dot(norm, lDir), 0.0);
        vec3 diffuse = diff * lightColor;
        
        // Shadow
        float shadow = ShadowCalculation(FragPosLightSpace, norm, lDir);
        
        vec3 lighting = (ambient + (1.0 - shadow) * diffuse);
        
        // Directional shading for boxy look
        float fakeDiffuse = abs(norm.y) * 1.0 + abs(norm.x) * 0.8 + abs(norm.z) * 0.6;
        lighting *= mix(1.0, fakeDiffuse, 0.4);
        
        FragColor = vec4(texColor.rgb * lighting, texColor.a);
    } else {
        FragColor = texColor;
    }
}
