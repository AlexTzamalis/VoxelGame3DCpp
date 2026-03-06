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
uniform float saturation;
uniform float contrast;

// ACES Film Tone Mapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 ld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
        
    float currentDepth = projCoords.z;
    
    // Tightened bias to reduce shimmering
    float bias = max(0.002 * (1.0 - dot(normal, ld)), 0.0008);
    
    // 5-tap PCF for smoother shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    const vec2 offsets[5] = vec2[](
        vec2(0, 0),
        vec2(1, 1), vec2(1, -1), vec2(-1, 1), vec2(-1, -1)
    );
    
    for(int i = 0; i < 5; ++i) {
        float pcfDepth = texture(shadowMap, projCoords.xy + offsets[i] * texelSize).r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
    shadow /= 5.0;
    
    return shadow * shadowIntensity;
}

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    if (texColor.a < 0.1) discard;
    
    // Vertex color contains AO and block tint
    texColor *= VertexColor; 
    
    // === WATER RENDERING ===
    if (VertexColor.a > 0.65 && VertexColor.a < 0.75) {
        if (waterMode == 1) {
            texColor.rgb *= vec3(0.12, 0.35, 0.85); // Slightly darker for realism
            texColor.a = 0.85;
            
            vec3 viewDir = normalize(cameraPos - FragPos);
            vec3 reflectDir = reflect(-normalize(lightDir), normalize(Normal));
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);
            texColor.rgb += vec3(spec * 0.6) * lightColor;
            
            if (ultraMode == 1) {
                float fresnel = pow(1.0 - max(dot(viewDir, normalize(Normal)), 0.0), 4.0);
                texColor.rgb = mix(texColor.rgb, skyColor * 0.5, fresnel * 0.4);
            }
        } else {
            texColor.rgb *= vec3(0.25, 0.45, 0.8);
            texColor.a = 0.75;
        }
    }
    
    vec3 N = normalize(Normal);
    
    // === DIRECTIONAL FACE SHADING ===
    float faceMul = 1.0;
    if (enableFaceShading == 1) {
        if (abs(N.y) > 0.5) faceMul = N.y > 0.0 ? 1.0 : 0.5;
        else if (abs(N.x) > 0.5) faceMul = 0.75;
        else if (abs(N.z) > 0.5) faceMul = 0.85;
    }
    
    // === LIGHTING ===
    vec3 ambient;
    if (enableShaders == 1) {
        float skyLight = clamp(0.5 + 0.5 * N.y, 0.0, 1.0);
        vec3 groundColor = vec3(0.12, 0.15, 0.1); 
        vec3 hemiAmbient = mix(groundColor, skyColor * 0.7, skyLight);
        ambient = hemiAmbient * (ultraMode == 1 ? 0.45 : 0.4);
        ambient = max(ambient, vec3(ambientBrightness));
    } else {
        ambient = vec3(max(0.45, ambientBrightness));
    }
    
    float diff = max(dot(N, normalize(lightDir)), 0.0);
    // Wrap lighting for softer moon/sun transitions in Ultra
    if (ultraMode == 1) {
        diff = mix(diff, diff * 0.5 + 0.5, 0.2); 
    }
    vec3 diffuse = diff * lightColor;
    
    float shadow = 0.0;
    if (enableShadows == 1) {
        shadow = ShadowCalculation(FragPosLightSpace, N, normalize(lightDir));
    }
    
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb * faceMul;
    
    // === POST-PROCESSING (Color Tuning) ===
    if (enableShaders == 1) {
        // Contrast
        result = mix(vec3(0.5), result, contrast);
        
        // Saturation
        float gray = dot(result, vec3(0.2126, 0.7152, 0.0722));
        result = mix(vec3(gray), result, saturation);
        
        // ACES Tone Mapping
        result = ACESFilm(result * 1.1);
    }
    
    // === FOG ===
    float distance = length(FragPos - cameraPos);
    vec4 finalColor;
    if (enableFog == 1) {
        float fogFactor = smoothstep(fogStart, fogEnd, distance);
        
        if (ultraMode == 1) {
            float heightFog = exp(-max(FragPos.y - 64.0, 0.0) * 0.015);
            fogFactor = mix(fogFactor, 1.0, heightFog * fogFactor * 0.4);
        }
        
        finalColor = vec4(mix(result, skyColor, fogFactor), texColor.a);
    } else {
        finalColor = vec4(result, texColor.a);
    }
    
    // Underwater
    if (enableShaders == 1 && isUnderwater == 1) {
        finalColor.rgb *= vec3(0.2, 0.55, 1.0);
        float ray = sin(FragPos.x * 0.3 + time * 1.8) * cos(FragPos.z * 0.3 + time * 1.4);
        finalColor.rgb += vec3(0.1, 0.4, 0.7) * max(ray, 0.0) * lightColor * 0.3;
    }
    
    FragColor = finalColor;
}
