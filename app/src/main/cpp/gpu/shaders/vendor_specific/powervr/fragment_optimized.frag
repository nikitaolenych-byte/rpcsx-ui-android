/**
 * RPCSX PowerVR-Optimized Fragment Shader
 * 
 * Optimizations:
 * - TBDR (Tile-Based Deferred Rendering) aware
 * - HSR (Hidden Surface Removal) friendly
 * - PVRTC texture support
 * - USC (Unified Shading Cluster) optimized
 */

#version 450

// Standard varyings (PowerVR handles interpolation efficiently)
layout(location = 0) in mediump vec2 fragTexCoord;
layout(location = 1) in mediump vec3 fragNormal;
layout(location = 2) in mediump vec4 fragColor;
layout(location = 3) in highp vec3 fragWorldPos;

layout(location = 0) out mediump vec4 outColor;

// Textures
layout(set = 0, binding = 0) uniform sampler2D texDiffuse;
layout(set = 0, binding = 1) uniform sampler2D texNormal;

// UBO
layout(set = 0, binding = 2, std140) uniform SceneData {
    mediump vec4 lightColor;
    highp vec4 lightPos;
    mediump vec4 ambientColor;
    mediump float specularPower;
    mediump float specularIntensity;
    mediump vec2 padding;
} scene;

// PowerVR-optimized lighting
// Uses simple Blinn-Phong which runs efficiently on USC
mediump vec3 powervrLighting(mediump vec3 N, mediump vec3 L, mediump vec3 V, mediump vec3 baseColor) {
    // Diffuse
    mediump float NdotL = max(dot(N, L), 0.0);
    mediump vec3 diffuse = baseColor * scene.lightColor.rgb * NdotL;
    
    // Specular (Blinn-Phong)
    mediump vec3 H = normalize(L + V);
    mediump float NdotH = max(dot(N, H), 0.0);
    mediump float spec = pow(NdotH, scene.specularPower) * scene.specularIntensity;
    mediump vec3 specular = scene.lightColor.rgb * spec;
    
    // Ambient
    mediump vec3 ambient = scene.ambientColor.rgb * baseColor;
    
    return ambient + diffuse + specular;
}

void main() {
    // Sample diffuse texture
    mediump vec4 diffuse = texture(texDiffuse, fragTexCoord);
    
    // Alpha test (PowerVR HSR works better with early discard)
    if (diffuse.a < 0.5) {
        discard;
    }
    
    // Normal
    mediump vec3 N = normalize(fragNormal);
    mediump vec3 normalMap = texture(texNormal, fragTexCoord).rgb * 2.0 - 1.0;
    N = normalize(N + normalMap * 0.3);
    
    // Lighting vectors
    highp vec3 L = normalize(scene.lightPos.xyz - fragWorldPos);
    mediump vec3 V = normalize(-fragWorldPos);
    
    // Calculate lighting
    mediump vec3 litColor = powervrLighting(N, L, V, diffuse.rgb);
    
    // Apply vertex color
    litColor *= fragColor.rgb;
    
    // Simple gamma (PowerVR handles this efficiently)
    litColor = pow(litColor, vec3(0.4545));
    
    outColor = vec4(litColor, diffuse.a * fragColor.a);
}
