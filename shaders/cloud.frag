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

uniform float sunSize;
uniform float sunIntensity;
uniform vec3 sunColor;
uniform float moonSize;
uniform float moonIntensity;
uniform float starDensity;
uniform float milkyWayIntensity;

float hash(float n) { return fract(sin(n) * 43758.5453123); }

float reflectNoise(vec3 p) {
    return fract(sin(dot(p.xz, vec2(12.9898, 78.233))) * 43758.5453);
}

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
    vec3 sampleCoord = (p + windOffset) * cloudScale * 0.45; // Increased scale slightly
    
    float noiseRaw = fbm3D(sampleCoord);
    noiseRaw = mix(noiseRaw, fbm3D(sampleCoord * 4.0 + p * 0.015), 0.3);
    
    // Sharper clouds: subtract more, then multiply higher
    float density = max(0.0, noiseRaw - (1.1 - cloudDensity)) * 6.0; 
    
    return vec2(density * verticalShape, noiseRaw);
}

vec3 getSkyColorVal(vec3 dir) {
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
    float hg = (1.0 - g*g) / pow(max(denom, 0.001), 1.5);
    vec3 sunGlowCol = sunColor * hg * 0.08;
    
    // Solar Disc (Safe Angular Radius Math)
    float sSize = clamp(sunSize, 0.002, 0.04);
    float sunDisc = smoothstep(cos(sSize), cos(sSize * 0.94), sunDot) * sunIntensity;
    vec3 currentSunCol = mix(sunColor, vec3(1.0, 0.35, 0.1), sunsetFact);
    
    // Moon (Safe Angular Radius Math)
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

    vec3 sky = skyBase + sunsetGlow - ozone + sunGlowCol + sunDisc * currentSunCol + moonDisc * currentMoonCol + stars;
    sky = mix(sky, vec3(0.001, 0.002, 0.005), nightFact);
    
    return max(vec3(0.0), sky);
}

void main() {
    vec4 clipPos = vec4(ScreenUV * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldDir = invViewProj * clipPos;
    vec3 rayDir = normalize(worldDir.xyz / worldDir.w - cameraPos);

    vec3 baseSky = getSkyColorVal(rayDir);
    float horizonFade = smoothstep(-0.15, 0.15, rayDir.y); // Smoother, deeper fade
    
    // Do NOT return early if rayDir.y < 0.01; let the distance calculation handle it or fade to sky
    
    float t_bottom = (cloudHeight - cameraPos.y) / rayDir.y;
    float t_top = ((cloudHeight + cloudThickness) - cameraPos.y) / rayDir.y;
    
    if (t_bottom < 0.0 && t_top < 0.0) { FragColor = vec4(baseSky, 1.0); return; }
    
    float t = max(0.0, t_bottom);
    float t_end = t_top;
    float maxDist = 30000.0;
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
    
    float finalFade = smoothstep(-0.05, 0.12, rayDir.y); // Allow some cloud below horizon for safety
    float distanceFade = smoothstep(maxDist, maxDist * 0.3, t);
    
    // Final output: Opaque skybox replacement with clouds
    // This ensures no seams with the clear colors or terrain fog
    vec3 res = mix(baseSky, cloudColorVal, (1.0 - transmittance) * distanceFade * finalFade);
    FragColor = vec4(res, 1.0);
}
