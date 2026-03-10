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
uniform float cloudDensity;   
uniform float cloudThickness; 
uniform int cloudSteps;       

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
    for(int i = 0; i < 5; i++) {
        f += weight * noise3D(p);
        p *= 2.4;
        weight *= 0.45;
    }
    return f;
}

vec2 getCloudDensity(vec3 p) {
    float hFract = (p.y - cloudHeight) / cloudThickness;
    float verticalShape = smoothstep(0.0, 0.15, hFract) * smoothstep(1.0, 0.45, hFract);
    
    vec3 windOffset = vec3(time * cloudSpeed * 15.0, 0.0, time * cloudSpeed * 8.0);
    vec3 sampleCoord = (p + windOffset) * cloudScale * 0.35;
    
    float noiseRaw = fbm3D(sampleCoord);
    noiseRaw = mix(noiseRaw, fbm3D(sampleCoord * 3.5 + p * 0.02), 0.2);
    
    float density = max(0.0, noiseRaw - (1.0 - cloudDensity)) * 4.0; 
    
    return vec2(density * verticalShape, noiseRaw);
}

vec3 getSkyColorVal(vec3 dir) {
    vec3 sunD = normalize(lightDir);
    float zenith = max(dir.y, 0.0);
    float sunDot = max(dot(dir, sunD), 0.0);

    vec3 zenithColor = vec3(0.05, 0.2, 0.5);
    vec3 horizonColor = vec3(0.4, 0.6, 0.8);
    vec3 skyBase = mix(horizonColor, zenithColor, pow(zenith, 0.5));
    
    float sunsetFactor = clamp(1.0 - sunD.y, 0.0, 1.0);
    vec3 sunsetGlow = vec3(1.0, 0.45, 0.1) * pow(1.0 - abs(dir.y), 3.0) * sunsetFactor;
    
    float nightFactor = smoothstep(0.1, -0.3, sunD.y);
    vec3 sky = skyBase + sunsetGlow;
    sky = mix(sky, vec3(0.005, 0.008, 0.02), nightFactor);
    
    return sky;
}

void main() {
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldDir = invViewProj * clipPos;
    vec3 rayDir = normalize(worldDir.xyz / worldDir.w - cameraPos);

    vec3 baseSky = getSkyColorVal(rayDir);
    if (rayDir.y < -0.01) { FragColor = vec4(baseSky, 1.0); return; }
    
    float t_bottom = (cloudHeight - cameraPos.y) / rayDir.y;
    float t_top = ((cloudHeight + cloudThickness) - cameraPos.y) / rayDir.y;
    
    if (t_bottom < 0.0 && t_top < 0.0) { FragColor = vec4(baseSky, 1.0); return; }
    
    float t = max(0.0, t_bottom);
    float t_end = t_top;
    float maxDist = 4500.0;
    if (t > maxDist) { FragColor = vec4(baseSky, 1.0); return; }
    t_end = min(t_end, maxDist);
    
    int stepsValue = cloudSteps; 
    float stepSize = (t_end - t) / float(stepsValue);
    float ditherVal = fract(sin(dot(ScreenUV.xy, vec2(12.9898, 78.233))) * 43758.5453);
    t += stepSize * ditherVal;
    
    float transmittance = 1.0;
    vec3 cloudColorVal = vec3(0.0);
    vec3 sunD = normalize(lightDir);
    float sunsetFact = clamp(1.0 - sunD.y, 0.0, 1.0);

    for (int i = 0; i < stepsValue; i++) {
        if (t > t_end || transmittance < 0.01) break;
        vec3 p = cameraPos + rayDir * t;
        vec2 dns = getCloudDensity(p);
        float density = dns.x;
        
        if (density > 0.001) {
            float densSun = getCloudDensity(p + sunD * 12.0).x;
            float sunLight = exp(-densSun * 1.5);
            
            // Henyey-Greenstein Silver Lining
            float g = 0.85; 
            float cosTheta = dot(rayDir, sunD);
            float hg = (1.0 - g*g) / pow(1.0 + g*g - 2.0*g*cosTheta, 1.5);
            
            vec3 sunCol = mix(lightColor, vec3(1.0, 0.45, 0.1), sunsetFact);
            vec3 amb = mix(vec3(0.3, 0.4, 0.6), skyColor, 0.4); 
            
            vec3 direct = sunCol * sunLight * (hg * 0.5 + 0.3) * 3.5;
            vec3 localCol = mix(amb, direct, density * 0.8);
            
            float alpha = (1.0 - exp(-density * stepSize * 0.12)) * transmittance;
            cloudColorVal += localCol * alpha;
            transmittance *= exp(-density * stepSize * 0.12);
        }
        t += stepSize;
    }
    
    float horizonFade = smoothstep(0.0, 0.12, rayDir.y);
    float distanceFade = smoothstep(maxDist, maxDist * 0.3, t);
    vec3 res = mix(baseSky, cloudColorVal, (1.0 - transmittance) * distanceFade * horizonFade);
    FragColor = vec4(res, 1.0);
}
