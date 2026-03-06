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

uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform float fogStart;
uniform float fogEnd;
uniform float time;
uniform int isUnderwater;

// Granular shader toggles
uniform int enableShaders;
uniform int enableShadows;
uniform float shadowIntensity;
uniform int enableFog;
uniform int enableFaceShading;
uniform float ambientBrightness;
uniform int waterMode;
uniform int ultraMode;
uniform float saturation;
uniform float contrast;

// --- ACES Film Tone Mapping ---
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- PCSS (Percentage Closer Soft Shadows) ---
// Simulates variable penumbra (softer shadows further away from caster)
float findBlocker(vec2 uv, float zReceiver) {
    float searchRadius = 0.01 * (zReceiver); // Radius expands with distance
    int blockers = 0;
    float avgBlockerDepth = 0.0;
    
    // Sample a small grid to find average blocker depth
    for(int i = -2; i <= 2; i++) {
        for(int j = -2; j <= 2; j++) {
            float d = texture(shadowMap, uv + vec2(i, j) * searchRadius * 0.5).r;
            if(d < zReceiver - 0.001) {
                blockers++;
                avgBlockerDepth += d;
            }
        }
    }
    if(blockers == 0) return -1.0;
    return avgBlockerDepth / float(blockers);
}

float PCSS(vec4 fragPosLightSpace, vec3 normal, vec3 ld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
        
    float zReceiver = projCoords.z;
    float bias = max(0.002 * (1.0 - dot(normal, ld)), 0.0008);
    
    // 1. Find blockers
    float avgBlockerDepth = findBlocker(projCoords.xy, zReceiver);
    if(avgBlockerDepth == -1.0) return 0.0;
    
    // 2. Calculate penumbra size
    // Width = (d_receiver - d_blocker) * light_size / d_blocker
    float penumbraSize = (zReceiver - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = clamp(penumbraSize * 0.1, 0.001, 0.02);
    
    // 3. PCF with variable radius
    float shadow = 0.0;
    const int SAMPLES = 16;
    vec2 PoissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38204446, 0.62256721), vec2(0.12984530, -0.15427459),
        vec2(0.70119661, 0.52841903), vec2(0.81146545, -0.41320341),
        vec2(-0.061440713, 0.051305282), vec2(0.38380316, -0.55470874),
        vec2(-0.45522521, -0.63582498), vec2(0.85404491, 0.10652077),
        vec2(-0.27218335, -0.040134442), vec2(0.53742981, 0.71602447)
    );

    for(int i = 0; i < SAMPLES; i++) {
        float d = texture(shadowMap, projCoords.xy + PoissonDisk[i] * filterRadius).r;
        if(zReceiver - bias > d) shadow += 1.0;
    }
    return (shadow / float(SAMPLES)) * shadowIntensity;
}

void main() {
    float distance = length(FragPos - cameraPos);
    
    // Optimization: Skip complex shadows at extreme distances
    if (distance > 300.0 && enableShadows == 1) {
       // Just fallback to simple diff
    }

    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;
    
    // VertexColor contains baked AO and face tints from CPU
    texColor *= VertexColor; 
    
    // --- Water Rendering ---
    if (VertexColor.a > 0.65 && VertexColor.a < 0.75) {
        if (waterMode == 1) {
            texColor.rgb *= vec3(0.1, 0.3, 0.8); // Richer deep blue
            texColor.a = 0.85;
            
            vec3 viewDir = normalize(cameraPos - FragPos);
            vec3 reflectDir = reflect(-normalize(lightDir), normalize(Normal));
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);
            texColor.rgb += vec3(spec * 0.7 * lightColor);
            
            if (ultraMode == 1) {
                float fresnel = pow(1.0 - max(dot(viewDir, normalize(Normal)), 0.0), 4.0);
                texColor.rgb = mix(texColor.rgb, skyColor * 0.4, fresnel * 0.5);
            }
        } else {
            texColor.rgb *= vec3(0.2, 0.4, 0.8);
            texColor.a = 0.7;
        }
    }
    
    vec3 N = normalize(Normal);
    
    // --- Face Shading (Voxel depth effect) ---
    float faceMul = 1.0;
    if (enableFaceShading == 1) {
        if (abs(N.y) > 0.5) faceMul = N.y > 0.0 ? 1.0 : 0.5; // Top/Bottom
        else if (abs(N.x) > 0.5) faceMul = 0.75; // East/West
        else if (abs(N.z) > 0.5) faceMul = 0.85; // North/South
    }
    
    // --- Ambient Lighting (Sky/Ground bounce) ---
    vec3 ambient;
    if (enableShaders == 1) {
        float skyLight = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
        vec3 groundColor = vec3(0.1, 0.12, 0.08); 
        vec3 skyBright = skyColor * 0.7;
        vec3 hemiAmbient = mix(groundColor, skyBright, skyLight);
        ambient = hemiAmbient * (ultraMode == 1 ? 0.45 : 0.35);
        ambient = max(ambient, vec3(ambientBrightness));
    } else {
        ambient = vec3(max(0.45, ambientBrightness));
    }
    
    // --- Diffuse Lighting ---
    float diff = max(dot(N, normalize(lightDir)), 0.0);
    if (ultraMode == 1) {
        diff = mix(diff, diff * 0.5 + 0.5, 0.15); // Wrap lighting for softness
    }
    vec3 diffuse = diff * lightColor;
    
    // --- Shadows (Soft PCSS in Ultra) ---
    float shadow = 0.0;
    if (enableShadows == 1) {
        if (ultraMode == 1 && distance < 150.0) {
            shadow = PCSS(FragPosLightSpace, N, normalize(lightDir));
        } else {
            // Standard PCF
            vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;
            float currentDepth = projCoords.z;
            float bias = max(0.002 * (1.0 - dot(N, normalize(lightDir))), 0.0008);
            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
            for(int x = -1; x <= 1; ++x) {
                for(int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                    shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
                }
            }
            shadow = (shadow / 9.0) * shadowIntensity;
        }
    }
    
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb * faceMul;
    
    // --- Tonemapping and Grading ---
    if (enableShaders == 1) {
        // Contrast
        result = mix(vec3(0.5), result, contrast);
        // Saturation (Luminance preserving)
        float lum = dot(result, vec3(0.2126, 0.7152, 0.0722));
        result = mix(vec3(lum), result, saturation);
        
        // Final Film Tone Map
        result = ACESFilm(result * 1.15);
    }
    
    // --- Fog ---
    vec4 finalColor;
    if (enableFog == 1) {
        float fogFactor = smoothstep(fogStart, fogEnd, distance);
        if (ultraMode == 1) {
            float heightFog = exp(-max(FragPos.y - 64.0, 0.0) * 0.02);
            fogFactor = mix(fogFactor, 1.0, heightFog * fogFactor * 0.35);
        }
        finalColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
    } else {
        finalColor = vec4(result, texColor.a);
    }
    
    // --- Underwater ---
    if (enableShaders == 1 && isUnderwater == 1) {
        finalColor.rgb *= vec3(0.2, 0.5, 1.0);
        float rays = sin(FragPos.x * 0.3 + time * 1.5) * cos(FragPos.z * 0.3 + time * 1.5);
        finalColor.rgb += vec3(0.1, 0.4, 0.7) * max(rays, 0.0) * 0.3;
    }
    
    FragColor = finalColor;
}
