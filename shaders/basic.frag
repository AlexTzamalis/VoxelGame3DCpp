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
float findBlocker(vec2 uv, float zReceiver) {
    float searchRadius = 0.01 * (zReceiver); 
    int blockers = 0;
    float avgBlockerDepth = 0.0;
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
    float avgBlockerDepth = findBlocker(projCoords.xy, zReceiver);
    if(avgBlockerDepth == -1.0) return 0.0;
    float penumbraSize = (zReceiver - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = clamp(penumbraSize * 0.1, 0.001, 0.02);
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

// --- Photon Sky Model (Simplified Atmospheric Scattering) ---
vec3 getPhotonSky(vec3 dir, vec3 sunDir) {
    float sunDot = dot(dir, sunDir);
    float zenith = max(dir.y, 0.0);
    
    // Rayleigh scattering approximation
    vec3 sky = mix(vec3(0.15, 0.25, 0.5), vec3(0.05, 0.4, 0.9), zenith);
    // Sunset/Sunrise horizon
    float horizon = 1.0 - abs(dir.y);
    vec3 sunset = vec3(1.0, 0.25, 0.05) * pow(horizon, 4.0) * max(sunDir.y + 0.2, 0.0) * 0.8;
    
    // Sun Disk
    float sun = smoothstep(0.998, 0.999, sunDot);
    vec3 sunColor = vec3(1.0, 0.9, 0.7) * 2.0;
    
    return sky + sunset + sun * sunColor;
}

void main() {
    float distance = length(FragPos - cameraPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 sunDir = normalize(lightDir);

    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;
    
    // Smooth Water (VertexColor contains AO)
    bool isWater = (VertexColor.a > 0.65 && VertexColor.a < 0.75);
    texColor *= VertexColor;
    
    if (isWater && waterMode == 1) {
        texColor.rgb *= vec3(0.15, 0.45, 0.8); 
        texColor.a = 0.82; // Slightly more opaque to hide "void" gaps if any
    }

    vec3 N = normalize(Normal);
    
    // --- Face Shading ---
    float faceMul = 1.0;
    if (enableFaceShading == 1) {
        if (abs(N.y) > 0.5) faceMul = N.y > 0.0 ? 1.0 : 0.45;
        else if (abs(N.x) > 0.5) faceMul = 0.7;
        else if (abs(N.z) > 0.5) faceMul = 0.85;
    }
    
    // --- Photon Ambient ---
    vec3 ambient;
    if (enableShaders == 1) {
        float skySide = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
        vec3 hemi = mix(vec3(0.04, 0.05, 0.06), skyColor * 0.7, skySide);
        ambient = hemi * (ultraMode == 1 ? 0.5 : 0.4);
        ambient = max(ambient, vec3(ambientBrightness));
    } else {
        ambient = vec3(max(0.45, ambientBrightness));
    }
    
    // --- Diffuse ---
    float diff = max(dot(N, sunDir), 0.0);
    if (ultraMode == 1) diff = mix(diff, diff * 0.4 + 0.6, 0.1); // Soft wrap
    vec3 diffuse = diff * lightColor;
    
    // --- Shadows ---
    float shadow = 0.0;
    if (enableShadows == 1) {
        if (ultraMode == 1 && distance < 150.0) {
            shadow = PCSS(FragPosLightSpace, N, sunDir);
        } else {
            vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w * 0.5 + 0.5;
            float bias = max(0.002 * (1.0 - dot(N, sunDir)), 0.0008);
            if (texture(shadowMap, projCoords.xy).r < projCoords.z - bias) shadow = 0.6;
            shadow *= shadowIntensity;
        }
    }
    
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb * faceMul;
    
    // --- Water Specular & Fresnel ---
    if (isWater && waterMode == 1) {
        vec3 reflectDir = reflect(-sunDir, N);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 256.0);
        result += lightColor * spec * 0.8;
        
        if (ultraMode == 1) {
            float fresnel = pow(1.0 - max(dot(viewDir, N), 0.0), 5.0);
            vec3 skyRefl = skyColor * 0.5; 
            result = mix(result, skyRefl, fresnel * 0.4);
        }
    }
    
    // --- Grading ---
    if (enableShaders == 1) {
        result = mix(vec3(0.5), result, contrast);
        float lum = dot(result, vec3(0.2126, 0.7152, 0.0722));
        result = mix(vec3(lum), result, saturation);
        result = ACESFilm(result * 1.05);
    }
    
    // --- Fog (Photon Style) ---
    if (enableFog == 1) {
        float fogFactor = smoothstep(fogStart, fogEnd, distance);
        if (ultraMode == 1) {
            float heightFog = exp(-max(FragPos.y - 64.0, 0.0) * 0.015);
            fogFactor = mix(fogFactor, 1.0, heightFog * fogFactor * 0.3);
        }
        result = mix(result, skyColor, fogFactor);
    }
    
    // --- Underwater ---
    if (isUnderwater == 1) {
        vec3 underwaterBlue = vec3(0.05, 0.2, 0.6);
        float depthFog = 1.0 - exp(-distance * 0.02);
        result = mix(result * vec3(0.3, 0.7, 1.0), underwaterBlue, depthFog);
        
        // Caustics / Ray effects
        float rays = sin(FragPos.x * 0.4 + time * 1.2) * cos(FragPos.z * 0.4 + time * 1.2);
        if (rays > 0.0) result += vec3(0.02, 0.1, 0.2) * rays;
    }
    
    FragColor = vec4(result, texColor.a);
}
