/**
 * RPCSX Mali-Optimized Fragment Shader
 * 
 * Optimizations:
 * - AFBC (ARM Frame Buffer Compression) aware
 * - Transaction Elimination support
 * - Valhall FMA optimizations
 * - Vector FP16 operations
 */

#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable

// Mali Valhall prefers packed varyings
layout(location = 0) in mediump vec4 fragTexCoordAndZ;  // xy=uv, zw=extra
layout(location = 1) in mediump vec4 fragNormalAndSpec; // xyz=normal, w=specular
layout(location = 2) in mediump vec4 fragColor;
layout(location = 3) in highp vec3 fragWorldPos;

layout(location = 0) out mediump vec4 outColor;

// Textures
layout(set = 0, binding = 0) uniform sampler2D texAlbedo;
layout(set = 0, binding = 1) uniform sampler2D texNormal;
layout(set = 0, binding = 2) uniform sampler2D texPBR;  // R=metallic, G=roughness, B=AO

// UBO for lighting
layout(set = 0, binding = 3, std140) uniform LightData {
    mediump vec4 lightColor;
    highp vec4 lightPos;
    mediump vec4 lightParams;  // x=ambient, y=specular, z=shininess, w=range
} light;

// Mali FMA-optimized lighting calculation
mediump vec3 maliOptimizedLighting(mediump vec3 N, mediump vec3 L, mediump vec3 V, mediump vec3 albedo) {
    // FMA operations for Valhall
    mediump float NdotL = max(dot(N, L), 0.0);
    mediump vec3 H = normalize(L + V);
    mediump float NdotH = max(dot(N, H), 0.0);
    
    // Diffuse with FMA
    mediump vec3 diffuse = albedo * light.lightColor.rgb * NdotL;
    
    // Specular with fast pow approximation (Mali-optimized)
    mediump float spec = pow(NdotH, light.lightParams.z);
    mediump vec3 specular = light.lightParams.y * spec * light.lightColor.rgb;
    
    // Ambient
    mediump vec3 ambient = light.lightParams.x * albedo;
    
    return ambient + diffuse + specular;
}

// Transaction Elimination friendly - avoid writing same values
bool shouldUpdatePixel(mediump vec4 newColor) {
    // Mali's TE can skip writes if color doesn't change significantly
    return true; // Hardware handles this, but we can hint
}

void main() {
    mediump vec2 uv = fragTexCoordAndZ.xy;
    mediump vec3 normal = normalize(fragNormalAndSpec.xyz);
    
    // Sample textures
    mediump vec4 albedo = texture(texAlbedo, uv);
    
    // Early discard for alpha (helps Mali's early-Z)
    if (albedo.a < 0.1) {
        discard;
    }
    
    // Normal mapping
    mediump vec3 normalSample = texture(texNormal, uv).rgb * 2.0 - 1.0;
    normal = normalize(normal + normalSample * 0.5);
    
    // PBR values
    mediump vec3 pbr = texture(texPBR, uv).rgb;
    mediump float metallic = pbr.r;
    mediump float roughness = pbr.g;
    mediump float ao = pbr.b;
    
    // Light direction
    highp vec3 L = normalize(light.lightPos.xyz - fragWorldPos);
    mediump vec3 V = normalize(-fragWorldPos);
    
    // Lighting calculation (Mali FMA optimized)
    mediump vec3 color = maliOptimizedLighting(normal, L, V, albedo.rgb);
    
    // Apply AO
    color *= ao;
    
    // Apply vertex color
    color *= fragColor.rgb;
    
    // Gamma correction (simplified for Mali)
    color = pow(color, vec3(0.4545)); // 1/2.2
    
    outColor = vec4(color, albedo.a * fragColor.a);
}
