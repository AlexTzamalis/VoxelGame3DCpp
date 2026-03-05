#version 330 core
in vec3 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertexColor;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform sampler2DArray textureAtlas;
uniform sampler2D shadowMap;
uniform vec3 lightDir;
uniform vec3 lightColor;

// Variables for Atmospheric Fog
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform float fogStart;
uniform float fogEnd;
uniform float time;
uniform int isUnderwater;
uniform int enableShaders;
uniform int enableShadows;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Shadows don't render outside the orthographic projection
    if(projCoords.z > 1.0)
        return 0.0;
        
    float currentDepth = projCoords.z;
    
    // Bias based on angle to fix shadow acne
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    // PCF (Percentage-Closer Filtering) for softer realistic shadow edges
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard; // Support true cut-out transparencies for leaves
    
    texColor *= VertexColor; 
    
    // Shader Mod Overrrides (Water specific rendering)
    if (enableShaders == 1 && VertexColor.a > 0.65 && VertexColor.a < 0.75) {
        // Deeper, vibrant ocean blue tint
        texColor.rgb *= vec3(0.2, 0.45, 1.0);
        texColor.a = 0.85; // Visually thicken the water depth
        
        // Specular highlight / Sun reflection on water
        vec3 viewDir = normalize(cameraPos - FragPos);
        vec3 reflectDir = reflect(-normalize(lightDir), normalize(Normal));
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        texColor.rgb += vec3(spec * 0.7) * lightColor; // Adds shiny sun/moon specular glare
    }
    
    vec3 N = normalize(Normal);
    
    // Radiosity-like ambient: The sky heavily influences the shadows!
    float skyLight = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
    vec3 ambient = (enableShaders == 1) 
                 ? mix(vec3(0.08, 0.12, 0.16), skyColor * 0.9, skyLight) * 0.6
                 : vec3(0.3); // Basic flat ambient
    
    // Directional explicitly tracking Sun/Moon
    float diff = max(dot(N, normalize(lightDir)), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Calculate the physical cascaded shadow coverage
    float shadow = 0.0;
    if (enableShadows == 1) {
        shadow = ShadowCalculation(FragPosLightSpace, N, normalize(lightDir));
    }
    
    // Add shadow directly into the multiplier but never let ambient be destroyed
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb;
    
    // Smooth fog for softer atmospheric blending natively merging with sky track
    float distance = length(FragPos - cameraPos);
    float fogFactor = smoothstep(fogStart, fogEnd, distance);
    
    vec4 finalColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
    
    // Shader Mod Overrrides (Underwater screen tint & rays)
    if (enableShaders == 1 && isUnderwater == 1) {
        // Thick ocean blue tint over everything viewed
        finalColor.rgb *= vec3(0.3, 0.65, 1.0);
        
        // Add fake light rays moving linearly over time across the block positions
        float ray = sin(FragPos.x * 0.4 + time * 1.5) * cos(FragPos.z * 0.4 + time * 1.2) * 0.5 + 0.5;
        vec3 rayColor = vec3(0.2, 0.5, 0.8) * ray * lightColor;
        finalColor.rgb += rayColor * 0.4;
    }
    
    FragColor = finalColor;
}
