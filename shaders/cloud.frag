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

// --- Noise functions ---
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

float cloudFBM(vec2 p) {
    float value = 0.0;
    float amplitude = 0.55;
    float frequency = 1.0;
    for (int i = 0; i < 6; i++) {
        value += amplitude * noise2D(p * frequency);
        frequency *= 2.2;
        amplitude *= 0.44;
    }
    return value;
}

void main() {
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldDir = invViewProj * clipPos;
    vec3 rayDir = normalize(worldDir.xyz / worldDir.w - cameraPos);
    
    if (rayDir.y < 0.005) discard;
    
    // Intersection with cloud plane
    float t = (cloudHeight - cameraPos.y) / rayDir.y;
    if (t < 0.0 || t > 5000.0) discard;
    
    vec3 hitPos = cameraPos + rayDir * t;
    vec2 wind = time * cloudSpeed * vec2(1.0, 0.2);
    vec2 uv = hitPos.xz * cloudScale + wind;
    
    float den = cloudFBM(uv);
    
    // High threshold for sparse, pillowy clouds
    float cloudMask = smoothstep(0.48, 0.72, den);
    if (cloudMask < 0.01) discard;
    
    // Self-shadowing approximation (sample density towards light)
    vec2 lightOffset = normalize(lightDir.xz) * 0.05;
    float denNear = cloudFBM(uv + lightOffset);
    float selfShadow = clamp(1.0 - (den - denNear) * 3.0, 0.3, 1.0);
    
    // Lighting: Sun scatter + Sky ambient
    float sunDot = max(dot(rayDir, normalize(lightDir)), 0.0);
    float sunScattering = pow(sunDot, 12.0) * 0.4; // Sunlight glow near sun
    
    vec3 cloudLit = mix(vec3(0.4, 0.45, 0.5), vec3(1.0, 0.98, 0.95), selfShadow);
    cloudLit *= lightColor;
    cloudLit += vec3(sunScattering) * lightColor;
    
    // Fade at horizon and distance
    float dist = length(hitPos.xz - cameraPos.xz);
    float fade = (1.0 - smoothstep(800.0, 2500.0, dist)) * smoothstep(0.005, 0.1, rayDir.y);
    
    FragColor = vec4(cloudLit, cloudMask * fade * 0.85);
}
