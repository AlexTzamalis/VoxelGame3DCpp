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

// Atmosphere expansions
uniform float sunSize;         
uniform float sunIntensity;    
uniform vec3 sunColor;
uniform float moonSize;
uniform float moonIntensity;
uniform float godRaysIntensity;
uniform float starDensity;
uniform float milkyWayIntensity;

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
    float bias = shadowBias;
    float avgBlockerDepth = findBlocker(projCoords.xy, zReceiver);
    if(avgBlockerDepth == -1.0) return 0.0;
    float penumbraSize = (zReceiver - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = clamp(penumbraSize * 0.1, 0.001, 0.02);
    float shadowCount = 0.0;
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
        if(zReceiver - bias > d) shadowCount += 1.0;
    }
    return (shadowCount / float(SAMPLES)) * shadowIntensity;
}

float reflectNoise(vec3 p) {
    return fract(sin(dot(p.xz, vec2(12.9898, 78.233))) * 43758.5453);
}

// --- Photon-Inspired Atmospheric Scattering ---
vec3 getSkyColor(vec3 dir) {
    vec3 sunD = normalize(lightDir);
    float zenith = max(dir.y, 0.0);
    float sunDot = max(dot(dir, sunD), 0.0);
    float moonDot = max(dot(dir, -sunD), 0.0);
    float sunsetFact = clamp(1.0 - sunD.y, 0.0, 1.0);
    float nightFact = smoothstep(0.05, -0.25, sunD.y);

    // Rayleigh scattering (Blue sky)
    vec3 zenithCol = vec3(0.01, 0.1, 0.4);
    vec3 horizonCol = vec3(0.4, 0.6, 0.9);
    vec3 skyBase = mix(horizonCol, zenithCol, pow(zenith, 0.6));
    
    // Ozone Absorption
    vec3 ozone = vec3(0.5, 0.1, 0.02) * sunsetFact * pow(1.0-zenith, 8.0);
    
    // Sunset Glow
    vec3 sunsetGlow = vec3(1.0, 0.4, 0.1) * pow(1.0 - abs(dir.y), 4.0) * sunsetFact;
    
    // Sun Glow (Halo)
    float g = 0.76; 
    float denom = 1.0 + g*g - 2.0*g*sunDot;
    float hgVal = (1.0 - g*g) / pow(max(denom, 0.001), 1.5);
    vec3 sunGlowCol = sunColor * hgVal * 0.08;
    
    // Solar Disc
    float sSize = clamp(sunSize, 0.002, 0.04);
    float sunDisc = smoothstep(cos(sSize), cos(sSize * 0.94), sunDot) * sunIntensity;
    vec3 currentSunCol = mix(sunColor, vec3(1.0, 0.35, 0.1), sunsetFact);
    
    // Moon
    float mSize = clamp(moonSize, 0.002, 0.035);
    float moonDisc = smoothstep(cos(mSize), cos(mSize * 0.94), moonDot) * moonIntensity;
    vec3 currentMoonCol = vec3(0.9, 0.95, 1.0);

    // Stars & Milky Way
    vec3 stars = vec3(0.0);
    if (nightFact > 0.01) {
        float sNoise = reflectNoise(dir * 200.0);
        if (sNoise > 1.0 - 0.002 * starDensity) {
            stars = vec3(1.0) * nightFact * sNoise;
        }
        float mw = reflectNoise(dir * 2.0) * 0.1 * milkyWayIntensity;
        stars += vec3(0.1, 0.15, 0.25) * mw * nightFact;
    }

    vec3 finalSky = skyBase + sunsetGlow - ozone + sunGlowCol + sunDisc * currentSunCol + moonDisc * currentMoonCol + stars;
    finalSky = mix(finalSky, vec3(0.001, 0.002, 0.005), nightFact);
    
    return max(vec3(0.0), finalSky);
}

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;

    // --- Early Shader Bypass (High Performance) ---
    if (enableShaders == 0) {
        float simpleFace = 1.0;
        if (abs(Normal.y) < 0.1) simpleFace = 0.8; // Basic side shading
        
        vec3 basicResult = texColor.rgb * VertexColor.rgb * simpleFace * ambientBrightness * 1.5;
        
        // Basic Water Tint in Performance Mode
        bool isWaterBasic = (VertexColor.a > 0.65 && VertexColor.a < 0.75);
        float alpha = texColor.a;
        if (isWaterBasic) {
            basicResult = mix(basicResult, vec3(0.1, 0.4, 0.6), 0.5);
            alpha = 0.8;
        }

        // Keep basic fog for depth in performance mode
        if (enableFog == 1) {
            float fDist = length(FragPos - cameraPos);
            float fFactor = smoothstep(fogStart * 0.8, fogEnd, fDist);
            basicResult = mix(basicResult, skyColor, fFactor);
        }

        FragColor = vec4(basicResult, alpha);
        return;
    }

    float distValue = length(FragPos - cameraPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 SD = normalize(lightDir);
    
    // Smooth Water (VertexColor channels ID)
    bool isWater = (VertexColor.a > 0.65 && VertexColor.a < 0.75);
    texColor.rgb *= VertexColor.rgb;
    
    // Photon Water - Volumetric Extinction (Beer-Lambert)
    if (isWater && waterMode == 1) {
        float depth = distValue; 
        vec3 absorption = vec3(0.4, 0.07, 0.02) * (1.1 - waterTransparency); 
        vec3 haze = vec3(0.02, 0.1, 0.2); 
        
        vec3 extinction = exp(-absorption * depth * 0.08);
        texColor.rgb = mix(haze, vec3(0.1, 0.4, 0.5), extinction); 
        texColor.a = 0.8 + 0.18 * waterTransparency; 
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
    vec3 ambientVal;
    if (enableShaders == 1) {
        float skySide = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
        vec3 hemi = mix(vec3(0.04, 0.05, 0.06), skyColor * 0.7, skySide);
        ambientVal = hemi * (ultraMode == 1 ? 0.5 : 0.4);
        ambientVal = max(ambientVal, vec3(ambientBrightness));
    } else {
        ambientVal = vec3(max(0.45, ambientBrightness));
    }
    
    // --- Diffuse ---
    float diffValue = max(dot(N, SD), 0.0);
    if (ultraMode == 1) diffValue = mix(diffValue, diffValue * 0.4 + 0.6, 0.1); // Soft wrap
    vec3 diffuseValue = diffValue * lightColor;
    
    // --- Shadows ---
    float shadowValue = 0.0;
    if (enableShadows == 1) {
        if (ultraMode == 1 && distValue < 150.0) {
            shadowValue = PCSS(FragPosLightSpace, N, SD);
        } else {
            vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w * 0.5 + 0.5;
            float biasValue = max(0.002 * (1.0 - dot(N, SD)), 0.0008);
            if (texture(shadowMap, projCoords.xy).r < projCoords.z - biasValue) shadowValue = 0.75 * shadowIntensity;
        }
    }
    
    // --- Volumetric Sunrays ---
    vec3 volumetricRays = vec3(0.0);
    if (enableShadows == 1 && ultraMode == 1 && isUnderwater == 0 && !isWater) {
        int stepsValue = 8;
        vec3 rayStep = (FragPos - cameraPos) / float(stepsValue);
        vec3 currentPos = cameraPos;
        float scatterValue = max(dot(viewDir, SD), 0.0);
        float miePhase = pow(scatterValue, 16.0) * 0.6 + 0.1;

        for (int i = 0; i < stepsValue; ++i) {
            currentPos += rayStep;
            vec4 lSpace = lightSpaceMatrix * vec4(currentPos, 1.0);
            vec3 proj = lSpace.xyz / lSpace.w * 0.5 + 0.5;
            if (proj.z < 1.0 && texture(shadowMap, proj.xy).r >= proj.z - 0.001) {
                volumetricRays += lightColor * miePhase;
            }
        }
        volumetricRays /= float(stepsValue);
    }
    
    vec3 resultColor = (ambientVal + (1.0 - shadowValue) * diffuseValue) * texColor.rgb * faceMul;
    resultColor += volumetricRays * godRaysIntensity; 
    
    // --- Water Overhaul ---
    if (isWater && waterMode == 1) {
        vec3 waterN = N;
        if (N.y > 0.9) {
            float w1 = sin(FragPos.x * 1.5 + time * 1.2 * waterWaveSpeed) * cos(FragPos.z * 1.5 + time * 0.8 * waterWaveSpeed);
            float w2 = sin(FragPos.x * 4.0 - time * 2.2 * waterWaveSpeed) * cos(FragPos.z * 3.5 - time * 1.8 * waterWaveSpeed);
            waterN = normalize(vec3(w1 * 0.15 * waterWaveHeight + w2 * 0.05 * waterWaveHeight, 1.0, w1 * 0.1 * waterWaveHeight + w2 * 0.07 * waterWaveHeight));
        }
        vec3 reflectDir = reflect(-viewDir, waterN);
        float specValue = pow(max(dot(reflectDir, SD), 0.0), 512.0) * 4.0;
        float fresnelValue = 0.02 + 0.98 * pow(1.0 - max(dot(viewDir, waterN), 0.0), 5.0);
        
        vec3 skyRefl = vec3(0.0);
        if (enableWaterReflections == 1) {
            skyRefl = getSkyColor(reflectDir);
            float cr = reflectNoise(FragPos + reflectDir * 100.0);
            skyRefl = mix(skyRefl, skyRefl * 1.2, smoothstep(0.4, 0.6, cr) * 0.2);
        }

        resultColor = mix(resultColor, skyRefl, fresnelValue * 0.85 * float(enableWaterReflections));
        resultColor += lightColor * specValue * (1.0 - shadowValue) * lightColor;
    }
    
    // --- Fog (Natural Atmospheric Integration) ---
    if (enableFog == 1) {
        float fogFactorValue = smoothstep(fogStart, fogEnd, distValue);
        
        // Dynamic Height Fog: denser near ground (64), clears as you climb
        float hRelative = FragPos.y - 64.0;
        float hFog = clamp(exp(-hRelative * fogHeightFalloff), 0.0, 1.0);
        
        // Blend fog factor: distance-based is primary, height-based adds atmosphere
        fogFactorValue = max(fogFactorValue, hFog * 0.15); // Subtle baseline haze
        
        // IMPORTANT: targetFogColor matches the sky at the horizon exactly
        vec3 fogDir = normalize(FragPos - cameraPos);
        vec3 targetFogColor = getSkyColor(fogDir);
        
        // Underwater murky fog override
        if (isUnderwater == 1) {
            vec3 waterFog = vec3(0.01, 0.1, 0.2);
            float uDist = clamp(distValue * 0.08, 0.0, 1.0);
            resultColor = mix(resultColor, waterFog, uDist);
            targetFogColor = waterFog;
            fogFactorValue = max(fogFactorValue, uDist);
        }
        
        resultColor = mix(resultColor, targetFogColor, fogFactorValue);
    }
    
    // --- Tonemapping & Contrast ---
    if (enableShaders == 1) {
        resultColor = mix(vec3(0.5), resultColor, contrast);
        float lumValue = dot(resultColor, vec3(0.2126, 0.7152, 0.0722));
        resultColor = mix(vec3(lumValue), resultColor, saturation);
        resultColor = ACESFilm(resultColor * 1.05);
    }
    
    FragColor = vec4(resultColor, texColor.a);
}
