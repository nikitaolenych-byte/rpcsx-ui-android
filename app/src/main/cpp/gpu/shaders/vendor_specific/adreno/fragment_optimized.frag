/**
 * RPCSX Adreno-Optimized Fragment Shader
 * 
 * Optimizations:
 * - FlexRender binning support
 * - UBWC (Universal Bandwidth Compression) friendly
 * - Half precision where possible
 * - Adreno-specific texture sampling
 */

#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable

// Adreno prefers explicit location bindings
layout(location = 0) in mediump vec2 fragTexCoord;
layout(location = 1) in mediump vec3 fragNormal;
layout(location = 2) in mediump vec4 fragColor;
layout(location = 3) in highp vec3 fragWorldPos;

layout(location = 0) out mediump vec4 outColor;

// Use descriptor set 0 for per-frame data
layout(set = 0, binding = 0) uniform sampler2D texAlbedo;
layout(set = 0, binding = 1) uniform sampler2D texNormal;
layout(set = 0, binding = 2) uniform sampler2D texSpecular;

// Push constants for fast updates (Adreno optimized)
layout(push_constant) uniform PushConstants {
    mediump vec4 lightColor;
    highp vec4 lightPos;
    mediump float ambientStrength;
    mediump float specularStrength;
    mediump float shininess;
    mediump float _padding;
} pc;

// Adreno-optimized helper functions
mediump vec3 calculateLighting(mediump vec3 normal, highp vec3 lightDir, mediump vec3 viewDir) {
    // Ambient
    mediump vec3 ambient = pc.ambientStrength * pc.lightColor.rgb;
    
    // Diffuse - use half precision dot product
    mediump float16_t diff = max(float16_t(dot(normal, lightDir)), float16_t(0.0));
    mediump vec3 diffuse = float(diff) * pc.lightColor.rgb;
    
    // Specular - Blinn-Phong (faster on Adreno)
    mediump vec3 halfwayDir = normalize(lightDir + viewDir);
    mediump float16_t spec = pow(max(float16_t(dot(normal, halfwayDir)), float16_t(0.0)), 
                                   float16_t(pc.shininess));
    mediump vec3 specular = pc.specularStrength * float(spec) * pc.lightColor.rgb;
    
    return ambient + diffuse + specular;
}

// Adreno texture sampling - use LOD bias for better performance
mediump vec4 sampleTextureOptimized(sampler2D tex, mediump vec2 uv) {
    // LOD bias of -0.5 helps with texture sharpness on Adreno
    return textureLod(tex, uv, -0.5);
}

void main() {
    // Sample textures with optimization
    mediump vec4 albedo = sampleTextureOptimized(texAlbedo, fragTexCoord);
    
    // Early alpha test for better binning
    if (albedo.a < 0.1) {
        discard;
    }
    
    // Normal mapping
    mediump vec3 normalMap = texture(texNormal, fragTexCoord).rgb;
    normalMap = normalize(normalMap * 2.0 - 1.0);
    mediump vec3 normal = normalize(fragNormal + normalMap);
    
    // Light calculations
    highp vec3 lightDir = normalize(pc.lightPos.xyz - fragWorldPos);
    mediump vec3 viewDir = normalize(-fragWorldPos);
    
    mediump vec3 lighting = calculateLighting(normal, lightDir, viewDir);
    
    // Specular map
    mediump float specMap = texture(texSpecular, fragTexCoord).r;
    lighting = mix(lighting, lighting * specMap, 0.5);
    
    // Final color
    outColor.rgb = albedo.rgb * lighting * fragColor.rgb;
    outColor.a = albedo.a * fragColor.a;
    
    // Adreno gamma correction (simplified for performance)
    outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
}
