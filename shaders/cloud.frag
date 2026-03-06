#version 330 core
in vec2 ScreenUV;
out vec4 FragColor;

uniform mat4 invViewProj;
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform float time;
uniform float cloudHeight;
uniform float cloudScale;
uniform float cloudSpeed;

// Hash functions for noise
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// FBM with 6 octaves for more detail
float cloudFBM(vec2 p) {
    float value = 0.0;
    float amplitude = 0.55;
    float frequency = 1.0;
    
    for (int i = 0; i < 6; i++) {
        value += amplitude * noise2D(p * frequency);
        frequency *= 2.1;
        amplitude *= 0.45;
    }
    return value;
}

void main() {
    // Reconstruct world-space ray from screen UV
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldDir = invViewProj * clipPos;
    vec3 rayDir = normalize(worldDir.xyz / worldDir.w - cameraPos);
    
    // Only render clouds above horizon
    if (rayDir.y < 0.01) {
        discard;
    }
    
    // Ray-plane intersection at cloud height
    float t = (cloudHeight - cameraPos.y) / rayDir.y;
    if (t < 0.0) {
        discard;
    }
    
    vec3 hitPos = cameraPos + rayDir * t;
    
    // Cloud shape using multi-layer FBM noise
    // Slow gentle drift with configurable speed
    vec2 windOffset = time * cloudSpeed * vec2(1.0, 0.3);
    vec2 cloudUV = hitPos.xz * cloudScale + windOffset;
    
    float cloudDensity = cloudFBM(cloudUV);
    
    // Higher threshold = sparser clouds with cleaner edges
    float cloudMask = smoothstep(0.45, 0.72, cloudDensity);
    
    if (cloudMask < 0.01) {
        discard;
    }
    
    // Multi-layer depth: use second noise sample for thickness illusion
    vec2 detailUV = cloudUV * 2.3 + vec2(0.5, 0.7);
    float detailNoise = cloudFBM(detailUV);
    float thickness = mix(0.3, 1.0, detailNoise);
    
    // Cloud lighting
    float sunDot = max(dot(vec3(0.0, 1.0, 0.0), normalize(lightDir)), 0.0);
    float sunInfluence = max(dot(rayDir, normalize(lightDir)), 0.0);
    
    // Base cloud color: warm whites during day, cool grays at night
    vec3 cloudBright = mix(vec3(0.85, 0.88, 0.92), vec3(1.0, 0.97, 0.92), sunDot);
    vec3 cloudDark = mix(vec3(0.35, 0.38, 0.42), vec3(0.55, 0.53, 0.52), sunDot);
    
    // Apply sun-side brightening
    cloudBright += vec3(sunInfluence * 0.15) * lightColor;
    
    // Shadow on cloud underside based on thickness
    vec3 cloudColor = mix(cloudDark, cloudBright * lightColor, thickness);
    
    // Silver lining effect at cloud edges near sun
    float edgeFactor = smoothstep(0.45, 0.55, cloudDensity);
    if (sunInfluence > 0.3) {
        cloudColor += vec3(0.15, 0.13, 0.1) * (1.0 - edgeFactor) * sunInfluence;
    }
    
    // Distance fade
    float fadeDist = length(hitPos.xz - cameraPos.xz);
    float distFade = 1.0 - smoothstep(600.0, 1500.0, fadeDist);
    
    // Horizon fade
    float horizonFade = smoothstep(0.01, 0.12, rayDir.y);
    
    float alpha = cloudMask * distFade * horizonFade * 0.8 * thickness;
    
    FragColor = vec4(cloudColor, alpha);
}
