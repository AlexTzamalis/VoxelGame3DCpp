#version 330 core
in vec2 ScreenUV;
out vec4 FragColor;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform float time;
uniform float cloudHeight;    
uniform float cloudScale;     
uniform float cloudSpeed;     
uniform float cloudDensity;   
uniform float cloudThickness; 
uniform int cloudSteps;       

uniform sampler2D galaxyTexture;
uniform sampler2D noiseTexture;
uniform sampler2D moonTexture;
uniform sampler2D starsTexture;
uniform sampler2D sunTexture;

uniform float sunSize;
uniform float sunIntensity;
uniform vec3 sunColor;
uniform float moonSize;
uniform float moonIntensity;
uniform float starDensity;
uniform float milkyWayIntensity;

uniform vec3 sunDir;
uniform int showClouds;

#define PI 3.14159265359

float henyey_greenstein_phase(float cos_theta, float g) {
    float g2 = g * g;
    return (1.0 / (4.0 * PI)) * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));
}

// --- Sync Dramatic Sky with basic.frag ---
vec3 getSkyColorVal(vec3 dir) {
    float sunDot = dot(dir, sunDir);
    float moonDot = dot(dir, -sunDir);
    float zenith = max(dir.y, 0.0);
    
    vec3 dayZenith = vec3(0.15, 0.45, 0.9);
    vec3 dayHorizon = vec3(0.5, 0.7, 1.0);
    vec3 sunsetOrange = vec3(1.0, 0.4, 0.1);
    vec3 sunsetPurple = vec3(0.6, 0.2, 0.5); 
    vec3 nightSky = vec3(0.01, 0.015, 0.04);
    
    float sunHeight = smoothstep(-0.25, 0.25, sunDir.y);
    float sunsetFactor = exp(-zenith * 3.0) * smoothstep(0.5, -0.4, abs(sunDir.y));
    
    vec3 dayBase = mix(dayHorizon, dayZenith, zenith);
    vec3 sky = mix(nightSky, dayBase, sunHeight);
    vec3 sunsetCol = mix(sunsetOrange, sunsetPurple, zenith * 2.0);
    sky = mix(sky, sunsetCol, sunsetFactor);
    
    float phaseM = henyey_greenstein_phase(sunDot, 0.85);
    vec3 sunHalo = vec3(1.0, 0.8, 0.5) * phaseM * 3.0 * sunHeight;
    sky += sunHalo * (1.0 - sunsetFactor * 0.5);
    
    float nightFact = clamp(1.0 - sunHeight, 0.0, 1.0);
    if (nightFact > 0.01) {
        vec2 skyUV = vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5, acos(dir.y) / PI);
        vec3 galaxy = texture(galaxyTexture, skyUV).rgb * milkyWayIntensity * 0.5;
        vec3 stars = texture(starsTexture, skyUV * 2.5).rgb * starDensity * 0.4;
        sky += (galaxy + stars) * nightFact;
    }
    
    if (sunDot > 0.985) {
        sky = mix(sky, vec3(1.0, 1.0, 0.9) * 20.0, smoothstep(0.985, 0.995, sunDot));
    }
    if (moonDot > 0.995) {
        sky = mix(sky, vec3(0.8, 0.8, 1.0) * 3.0, smoothstep(0.995, 0.998, moonDot));
    }

    return max(vec3(0.0), sky);
}

// --- Billowy Photon-Style Density ---
float getCloudDensity(vec3 p) {
    vec3 pScaled = p * cloudScale * 0.005;
    vec2 uv = pScaled.xz + time * cloudSpeed * 0.05;
    
    // Multi-octave billowy noise
    float base = texture(noiseTexture, uv * 0.2).r;
    float detail = texture(noiseTexture, uv * 0.8).r;
    float shape = mix(base, detail, 0.4);
    
    // Coverage
    float dens = smoothstep(1.2 - cloudDensity, 1.5 - cloudDensity, shape);
    
    // Height Shaping (Photon 3D look)
    float h = (p.y - cloudHeight) / cloudThickness;
    dens *= smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.7, h);
    
    return dens;
}

void main() {
    mat3 invView = mat3(inverse(view));
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 viewDirVec = inverse(projection) * clipPos;
    vec3 rayDir = normalize(invView * viewDirVec.xyz);

    vec3 baseSky = getSkyColorVal(rayDir);
    
    if (showClouds == 0 || rayDir.y < 0.01) {
        FragColor = vec4(baseSky, 1.0);
        return;
    }

    // --- High-Fidelity Volumetric Raymarching ---
    float startDist = (cloudHeight - cameraPos.y) / rayDir.y;
    float endDist = (cloudHeight + cloudThickness - cameraPos.y) / rayDir.y;
    
    float t = max(0.0, startDist);
    float tEnd = min(30000.0, endDist);
    
    if (t > tEnd) {
        FragColor = vec4(baseSky, 1.0); return;
    }

    int steps = clamp(cloudSteps, 24, 64);
    float stepSize = (tEnd - t) / float(steps);
    float transmittance = 1.0;
    vec3 lightScattering = vec3(0.0);
    
    for (int i = 0; i < steps; i++) {
        if (transmittance < 0.01) break;
        
        vec3 p = cameraPos + rayDir * t;
        float d = getCloudDensity(p);
        
        if (d > 0.01) {
            // Photon-inspired light integration
            float shadowSample = getCloudDensity(p + normalize(sunDir) * 20.0);
            float shadowing = exp(-shadowSample * 3.0);
            
            vec3 envLight = skyColor * 0.5;
            vec3 directLight = lightColor * shadowing * sunIntensity * 2.5;
            
            vec3 voxelLight = mix(envLight, directLight, 0.8);
            
            float sigma_a = d * 0.2;
            float step_trans = exp(-sigma_a * stepSize);
            
            lightScattering += transmittance * (1.0 - step_trans) * voxelLight;
            transmittance *= step_trans;
        }
        t += stepSize;
    }

    vec3 final = baseSky * transmittance + lightScattering;
    FragColor = vec4(final, 1.0);
}
