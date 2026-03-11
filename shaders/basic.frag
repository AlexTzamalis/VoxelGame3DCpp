#version 330 core
in vec3 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertexColor;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform sampler2DArray textureAtlas;
uniform sampler2D shadowMap;
uniform vec3 sunDir; 
uniform vec3 lightDir; 
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform vec3 cameraPos;

uniform sampler2D galaxyTexture;
uniform sampler2D noiseTexture;
uniform sampler2D moonTexture;
uniform sampler2D starsTexture;
uniform sampler2D sunTexture;
uniform float fogStart;
uniform float fogEnd;
uniform float time;
uniform int isUnderwater;

uniform int enableShaders;
uniform int enableShadows;
uniform float shadowIntensity;
uniform float shadowSoftness;
uniform float shadowBias;
uniform int enableFog;
uniform vec3 fogColor;
uniform float fogHeightFalloff;
uniform int enableFaceShading;
uniform float ambientBrightness;
uniform int waterMode;
uniform float waterTransparency;
uniform float waterWaveSpeed;
uniform float waterWaveHeight;
uniform int enableWaterReflections;
uniform int ultraMode;
uniform float saturation;
uniform float contrast;
uniform mat4 lightSpaceMatrix;

uniform float sunSize;         
uniform float sunIntensity;    
uniform vec3 sunColor;
uniform float moonSize;
uniform float moonIntensity;
uniform float godRaysIntensity;
uniform float starDensity;
uniform float milkyWayIntensity;

#define PI 3.14159265359

// --- Photon-Style Helper: Henyey-Greenstein ---
float henyey_greenstein_phase(float cos_theta, float g) {
    float g2 = g * g;
    return (1.0 / (4.0 * PI)) * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));
}

// --- ACES Film Tone Mapping ---
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- Multi-octave Gerstner wave for choppy water ---
struct Wave {
    vec2 dir;
    float wavelength;
    float amplitude;
    float speed;
};

vec3 calculateGerstner(Wave w, vec2 p, float t, inout vec3 tangent, inout vec3 binormal) {
    float k = 2.0 * PI / w.wavelength;
    float c = sqrt(9.8 / k);
    vec2 d = normalize(w.dir);
    float f = k * (dot(d, p) - (c + w.speed) * t);
    float a = w.amplitude;

    tangent += vec3(-d.x * d.x * (a * k * sin(f)), d.x * (a * k * cos(f)), -d.x * d.y * (a * k * sin(f)));
    binormal += vec3(-d.x * d.y * (a * k * sin(f)), d.y * (a * k * cos(f)), -d.y * d.y * (a * k * sin(f)));

    return vec3(d.x * (a * cos(f)), a * sin(f), d.y * (a * cos(f)));
}

// --- Dramatic Photon-Inspired Sky ---
vec3 getSkyColor(vec3 dir) {
    float sunDot = dot(dir, sunDir);
    float moonDot = dot(dir, -sunDir);
    float zenith = max(dir.y, 0.0);
    
    // 1. Atmosphere Layers
    vec3 dayZenith = vec3(0.15, 0.45, 0.9);
    vec3 dayHorizon = vec3(0.5, 0.7, 1.0);
    vec3 sunsetOrange = vec3(1.0, 0.4, 0.1);
    vec3 sunsetPurple = vec3(0.6, 0.2, 0.5); // Dramatic purple tier
    vec3 nightSky = vec3(0.01, 0.015, 0.04);
    
    float sunHeight = smoothstep(-0.25, 0.25, sunDir.y);
    float sunsetFactor = exp(-zenith * 3.0) * smoothstep(0.5, -0.4, abs(sunDir.y));
    
    // Base sky gradient
    vec3 dayBase = mix(dayHorizon, dayZenith, zenith);
    vec3 sky = mix(nightSky, dayBase, sunHeight);
    
    // Dramatic Sunset/Sunrise
    vec3 sunsetCol = mix(sunsetOrange, sunsetPurple, zenith * 2.0);
    sky = mix(sky, sunsetCol, sunsetFactor);
    
    // 2. Photon Scattering (Soft Halo)
    float phaseM = henyey_greenstein_phase(sunDot, 0.85);
    vec3 sunHalo = vec3(1.0, 0.8, 0.5) * phaseM * 3.0 * sunHeight;
    sky += sunHalo * (1.0 - sunsetFactor * 0.5);
    
    // 3. Night Features
    float nightFact = clamp(1.0 - sunHeight, 0.0, 1.0);
    if (nightFact > 0.01) {
        vec2 skyUV = vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5, acos(dir.y) / PI);
        vec3 galaxy = texture(galaxyTexture, skyUV).rgb * milkyWayIntensity * 0.5;
        vec3 stars = texture(starsTexture, skyUV * 2.5).rgb * starDensity * 0.4;
        sky += (galaxy + stars) * nightFact;
    }
    
    // 4. Sun & Moon Sprites
    if (sunDot > 0.985) {
        float sunMask = smoothstep(0.985, 0.995, sunDot);
        sky = mix(sky, vec3(1.0, 1.0, 0.9) * 20.0, sunMask);
    }
    
    if (moonDot > 0.995) {
        float moonMask = smoothstep(0.995, 0.998, moonDot);
        sky = mix(sky, vec3(0.8, 0.8, 1.0) * 3.0, moonMask);
    }

    return max(vec3(0.0), sky);
}

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;

    float distValue = length(FragPos - cameraPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 SD = normalize(lightDir);
    
    bool isWater = (VertexColor.a > 0.65 && VertexColor.a < 0.75);
    texColor.rgb *= VertexColor.rgb;
    
    // Water remains improved (translucent cyan)
    if (isWater) {
        texColor.rgb = mix(vec3(0.02, 0.25, 0.4), vec3(0.1, 0.6, 0.65), 0.5); 
        texColor.a = 0.5 * waterTransparency;
    }

    vec3 N = normalize(Normal);
    if (length(Normal) < 0.01) N = vec3(0, 1, 0); 
    
    if (isWater && waterMode == 1) {
        vec2 coord = FragPos.xz;
        float t = time * waterWaveSpeed;
        vec3 tangent = vec3(1, 0, 0);
        vec3 binormal = vec3(0, 0, 1);
        vec3 offset = vec3(0.0);
        
        Wave waves[4];
        waves[0] = Wave(vec2(1.0, 0.2), 30.0, 0.5, 0.5);
        waves[1] = Wave(vec2(0.4, 0.8), 15.0, 0.25, 0.8);
        waves[2] = Wave(vec2(-0.5, 0.7), 8.0, 0.12, 1.2);
        waves[3] = Wave(vec2(0.8, -0.3), 5.0, 0.08, 1.5);
        
        for(int i=0; i<4; i++) {
            offset += calculateGerstner(waves[i], coord, t, tangent, binormal);
        }
        N = normalize(cross(binormal, tangent));
        float micro = texture(noiseTexture, coord * 0.1 + time * 0.05).r;
        N = normalize(mix(N, vec3(micro - 0.5, 2.0, 0.0), 0.05));
    }

    float faceMul = 1.0;
    if (enableFaceShading == 1) {
        if (abs(N.y) > 0.5) faceMul = N.y > 0.0 ? 1.0 : 0.55;
        else if (abs(N.x) > 0.5) faceMul = 0.75;
        else if (abs(N.z) > 0.5) faceMul = 0.85;
    }
    
    float diff = max(dot(N, SD), 0.0);
    vec3 ambient = vec3(ambientBrightness);
    if (ultraMode == 1) ambient = mix(vec3(0.05, 0.07, 0.12), skyColor * 0.12, max(N.y, 0.0));
    
    float shadow = 0.0;
    if (enableShadows == 1) {
        vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w * 0.5 + 0.5;
        if (projCoords.z <= 1.0) {
            float bias = max(0.003 * (1.0 - dot(N, SD)), 0.0008);
            float d = texture(shadowMap, projCoords.xy).r;
            shadow = (projCoords.z - bias > d) ? shadowIntensity : 0.0;
        }
    }

    vec3 result = (ambient + (1.0 - shadow) * diff * lightColor) * texColor.rgb * faceMul;
    
    if (isWater && waterMode == 1) {
        vec3 R = reflect(-viewDir, N);
        vec3 skyRefl = getSkyColor(R);
        float fresnel = 0.03 + 0.97 * pow(1.0 - max(dot(N, viewDir), 0.0), 5.0);
        result = mix(result, skyRefl, fresnel * waterTransparency);
        vec3 H = normalize(SD + viewDir);
        float spec = pow(max(dot(N, H), 0.0), 128.0);
        result += lightColor * spec * 3.0 * waterTransparency;
    }

    if (enableFog == 1) {
        float fDist = length(FragPos - cameraPos);
        float fFactor = smoothstep(fogStart, fogEnd, fDist);
        vec3 fCol = getSkyColor(normalize(FragPos - cameraPos));
        result = mix(result, fCol, fFactor);
    }
    
    if (enableShaders == 1) {
        result = ACESFilm(result * 1.05);
        result = mix(vec3(dot(result, vec3(0.299, 0.587, 0.114))), result, saturation);
    }

    FragColor = vec4(result, texColor.a);
}
