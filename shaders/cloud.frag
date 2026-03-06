#version 330 core
in vec2 ScreenUV;
out vec4 FragColor;

uniform mat4 invViewProj;
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform float time;
uniform float cloudHeight;    // Base altitude of clouds
uniform float cloudScale;     // Overall noise scale
uniform float cloudSpeed;     // Wind speed

float hash(float n) { return fract(sin(n) * 43758.5453123); }

float noise3D(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 157.0 + 113.0 * p.z;
    return mix(
        mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
            mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
        mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
            mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

float fbm3D(vec3 p) {
    float f = 0.0;
    float weight = 0.5;
    for(int i = 0; i < 4; i++) {
        f += weight * noise3D(p);
        p *= 2.5;
        weight *= 0.4;
    }
    return f;
}

vec2 getCloudDensity(vec3 p) {
    float thickness = 40.0; // depth of the cloud layer
    float heightFract = (p.y - cloudHeight) / thickness;
    
    // Smooth bottom edge and rounded pillowy top
    float verticalShape = smoothstep(0.0, 0.15, heightFract) * smoothstep(1.0, 0.4, heightFract);
    
    // Wind animation
    vec3 windOffset = vec3(time * cloudSpeed * 2.0, 0.0, time * cloudSpeed * 1.0);
    vec3 sampleCoord = (p + windOffset) * cloudScale * 0.04;
    
    float noiseRaw = fbm3D(sampleCoord);
    
    // Cloud coverage threshold (determines sky coverage vs patchiness)
    float coverage = 0.6; // Higher = less clouds
    
    float density = max(0.0, noiseRaw - coverage) * 3.0; // Boost density mathematically
    return vec2(density * verticalShape, noiseRaw);
}

void main() {
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldDir = invViewProj * clipPos;
    vec3 rayDir = normalize(worldDir.xyz / worldDir.w - cameraPos);
    
    // Early exit if looking downwards
    if (rayDir.y < 0.01) discard;
    
    // Cloud layer bounds
    float t_bottom = (cloudHeight - cameraPos.y) / rayDir.y;
    float cloudThickness = 40.0;
    float t_top = ((cloudHeight + cloudThickness) - cameraPos.y) / rayDir.y;
    
    if (t_bottom < 0.0 || t_top < 0.0) discard;
    
    float t = max(0.0, t_bottom);
    float t_end = t_top;
    
    // Limit horizon draw distance for performance
    float maxDist = 3000.0;
    if (t > maxDist) discard;
    t_end = min(t_end, maxDist);
    
    int steps = 24; 
    float stepSize = (t_end - t) / float(steps);
    
    // Randomize starting ray step to eliminate banding
    float dither = fract(sin(dot(ScreenUV.xy, vec2(12.9898, 78.233))) * 43758.5453);
    t += stepSize * dither;
    
    float totalDensity = 0.0;
    float lightTransmittance = 1.0;
    vec3 cloudColor = vec3(0.0);
    
    vec3 sunDir = normalize(lightDir);
    float sunScattering = pow(max(dot(rayDir, sunDir), 0.0), 16.0) * 0.5;
    
    // Raymarching Loop
    for (int i = 0; i < steps; i++) {
        if (t > t_end || lightTransmittance < 0.01) break;
        
        vec3 p = cameraPos + rayDir * t;
        vec2 dns = getCloudDensity(p);
        float density = dns.x;
        
        if (density > 0.01) {
            // Sample density slightly towards the sun for self-shadowing
            float densSun = getCloudDensity(p + sunDir * 10.0).x;
            float sunLight = clamp(exp(-densSun * 2.0), 0.0, 1.0);
            
            // Base ambient light vs direct sun
            vec3 ambient = mix(vec3(0.45, 0.5, 0.6), skyColor, 0.3); // Ambient
            vec3 direct = lightColor * sunLight * (1.0 + sunScattering);
            vec3 localColor = ambient + direct;
            
            // Beer's law integration
            float transmittance = exp(-density * stepSize * 0.05); // extinction coeff
            float alpha = (1.0 - transmittance) * lightTransmittance;
            
            cloudColor += localColor * alpha;
            lightTransmittance *= transmittance;
            totalDensity += density * stepSize;
        }
        
        t += stepSize;
    }
    
    if (totalDensity < 0.01) discard;
    
    // Edge horizon fade
    float distSq = t * t;
    float fade = smoothstep(maxDist*maxDist, (maxDist*0.3)*(maxDist*0.3), distSq);
    
    FragColor = vec4(cloudColor, (1.0 - lightTransmittance) * fade);
}
