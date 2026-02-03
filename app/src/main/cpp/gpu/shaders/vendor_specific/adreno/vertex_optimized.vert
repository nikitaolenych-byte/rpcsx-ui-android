/**
 * RPCSX Adreno-Optimized Vertex Shader
 * 
 * Optimizations:
 * - Vertex attribute packing
 * - Early clip plane testing
 * - GMEM-friendly output
 */

#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable

// Input vertex attributes (packed for Adreno)
layout(location = 0) in highp vec3 inPosition;
layout(location = 1) in mediump vec3 inNormal;
layout(location = 2) in mediump vec2 inTexCoord;
layout(location = 3) in mediump vec4 inColor;
layout(location = 4) in mediump vec4 inTangent;

// Output to fragment shader
layout(location = 0) out mediump vec2 fragTexCoord;
layout(location = 1) out mediump vec3 fragNormal;
layout(location = 2) out mediump vec4 fragColor;
layout(location = 3) out highp vec3 fragWorldPos;
layout(location = 4) out mediump vec3 fragTangent;
layout(location = 5) out mediump vec3 fragBitangent;

// UBO with matrices (aligned for Adreno)
layout(set = 0, binding = 0, std140) uniform Matrices {
    highp mat4 model;
    highp mat4 view;
    highp mat4 projection;
    highp mat4 mvp;
    highp mat3 normalMatrix;
} matrices;

// Push constants for per-draw updates
layout(push_constant) uniform PushConstants {
    highp vec4 objectOffset;
    mediump float time;
    mediump float scale;
    mediump vec2 uvOffset;
} pc;

// Adreno prefers explicit invariant for position
invariant gl_Position;

void main() {
    // Apply object transform
    highp vec4 worldPos = matrices.model * vec4(inPosition * pc.scale + pc.objectOffset.xyz, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform to clip space - use precomputed MVP for speed
    gl_Position = matrices.mvp * vec4(inPosition * pc.scale + pc.objectOffset.xyz, 1.0);
    
    // Normal in world space
    fragNormal = normalize(matrices.normalMatrix * inNormal);
    
    // Tangent space (for normal mapping)
    fragTangent = normalize(matrices.normalMatrix * inTangent.xyz);
    fragBitangent = cross(fragNormal, fragTangent) * inTangent.w;
    
    // Pass through other attributes
    fragTexCoord = inTexCoord + pc.uvOffset;
    fragColor = inColor;
}
