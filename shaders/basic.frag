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

// Cloud uniforms
uniform float cloudHeight;
uniform float cloudScale;

// Simple 3D noise for volumetric fog
float hash(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yxz + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    return mix(
        mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x),
            mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
            mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
        f.z);
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise3D(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 ld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
        
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, ld)), 0.001);
    
    // PCF soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow * shadowIntensity;
}

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;
    
    texColor *= VertexColor; 
    
    // === WATER RENDERING ===
    if (VertexColor.a > 0.65 && VertexColor.a < 0.75) {
        if (waterMode == 1) {
            texColor.rgb *= vec3(0.15, 0.4, 0.95);
            texColor.a = 0.82;
            
            vec3 viewDir = normalize(cameraPos - FragPos);
            vec3 reflectDir = reflect(-normalize(lightDir), normalize(Normal));
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
            texColor.rgb += vec3(spec * 0.8) * lightColor;
            
            // Fresnel effect — more reflective at grazing angles
            if (ultraMode == 1) {
                float fresnel = pow(1.0 - max(dot(viewDir, normalize(Normal)), 0.0), 3.0);
                texColor.rgb = mix(texColor.rgb, skyColor * 0.6, fresnel * 0.4);
            }
        } else {
            texColor.rgb *= vec3(0.3, 0.5, 0.9);
            texColor.a = 0.75;
        }
    }
    
    vec3 N = normalize(Normal);
    
    // === DIRECTIONAL FACE SHADING (Minecraft-style) ===
    float faceMul = 1.0;
    if (enableFaceShading == 1) {
        if (abs(N.y) > 0.5) {
            faceMul = N.y > 0.0 ? 1.0 : 0.5;
        } else if (abs(N.x) > 0.5) {
            faceMul = 0.7;
        } else if (abs(N.z) > 0.5) {
            faceMul = 0.8;
        }
    }
    
    // === AMBIENT LIGHTING ===
    vec3 ambient;
    if (enableShaders == 1) {
        float skyLight = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
        vec3 skyAmbient = mix(vec3(ambientBrightness * 0.4, ambientBrightness * 0.45, ambientBrightness * 0.6), 
                              skyColor * 0.85, skyLight);
        ambient = skyAmbient * 0.7;
        ambient = max(ambient, vec3(ambientBrightness));
        
        // Ultra mode: hemisphere ambient with ground bounce
        if (ultraMode == 1) {
            vec3 groundColor = vec3(0.15, 0.2, 0.1); // Green-ish ground bounce
            vec3 hemiAmbient = mix(groundColor, skyColor * 0.6, skyLight * 0.5 + 0.5);
            ambient = hemiAmbient * 0.5;
            ambient = max(ambient, vec3(ambientBrightness));
        }
    } else {
        ambient = vec3(max(0.4, ambientBrightness));
    }
    
    // === DIRECTIONAL SUN/MOON LIGHT ===
    float diff = max(dot(N, normalize(lightDir)), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Ultra mode: wrap lighting for softer transitions
    if (ultraMode == 1) {
        float wrapDiff = max(dot(N, normalize(lightDir)) * 0.5 + 0.5, 0.0);
        diffuse = wrapDiff * lightColor * 0.7;
    }
    
    // === SHADOWS ===
    float shadow = 0.0;
    if (enableShadows == 1) {
        shadow = ShadowCalculation(FragPosLightSpace, N, normalize(lightDir));
    }
    
    // Combine lighting
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb * faceMul;
    
    // === FOG ===
    float distance = length(FragPos - cameraPos);
    vec4 finalColor;
    if (enableFog == 1) {
        float fogFactor = smoothstep(fogStart, fogEnd, distance);
        
        // Ultra mode: Volumetric fog with height falloff
        if (ultraMode == 1) {
            float heightFog = exp(-max(FragPos.y - 60.0, 0.0) * 0.02);
            fogFactor = mix(fogFactor, 1.0, heightFog * fogFactor * 0.3);
        }
        
        finalColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
    } else {
        finalColor = vec4(result, texColor.a);
    }
    
    // === UNDERWATER EFFECTS ===
    if (enableShaders == 1 && isUnderwater == 1) {
        finalColor.rgb *= vec3(0.3, 0.65, 1.0);
        float ray = sin(FragPos.x * 0.4 + time * 1.5) * cos(FragPos.z * 0.4 + time * 1.2) * 0.5 + 0.5;
        vec3 rayColor = vec3(0.2, 0.5, 0.8) * ray * lightColor;
        finalColor.rgb += rayColor * 0.4;
    }
    
    // Ultra mode: Subtle bloom approximation on bright areas
    if (ultraMode == 1) {
        float brightness = dot(finalColor.rgb, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > 0.9) {
            finalColor.rgb += (finalColor.rgb - vec3(0.9)) * 0.3;
        }
    }
    
    FragColor = finalColor;
}
