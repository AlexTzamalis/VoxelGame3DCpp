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

// Hash function for noise
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// 2D Value noise
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

// FBM for cloud shapes
float cloudFBM(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 5; i++) {
        value += amplitude * noise2D(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
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
    
    // Cloud shape using FBM noise
    vec2 cloudUV = hitPos.xz * cloudScale;
    cloudUV += time * vec2(0.3, 0.1); // Wind drift
    
    float cloudDensity = cloudFBM(cloudUV);
    
    // Shape the clouds: threshold with smooth edges
    float cloudMask = smoothstep(0.35, 0.65, cloudDensity);
    
    if (cloudMask < 0.01) {
        discard;
    }
    
    // Cloud lighting: bright on sun side, dark on shadow side
    float sunDot = max(dot(vec3(0.0, 1.0, 0.0), normalize(lightDir)), 0.0);
    vec3 cloudLit = mix(vec3(0.6, 0.65, 0.7), vec3(1.0, 0.98, 0.95), sunDot);
    cloudLit *= lightColor;
    
    // Dark underside shading
    float thickness = cloudFBM(cloudUV * 1.3 + 0.5);
    vec3 cloudColor = mix(cloudLit * 0.6, cloudLit, 1.0 - thickness * 0.5);
    
    // Distance fade
    float fadeDist = length(hitPos.xz - cameraPos.xz);
    float distFade = 1.0 - smoothstep(800.0, 2000.0, fadeDist);
    
    // Horizon fade (blend with sky near edges)
    float horizonFade = smoothstep(0.01, 0.15, rayDir.y);
    
    float alpha = cloudMask * distFade * horizonFade * 0.85;
    
    FragColor = vec4(cloudColor, alpha);
}
