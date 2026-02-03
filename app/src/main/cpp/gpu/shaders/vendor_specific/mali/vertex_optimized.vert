/**
 * RPCSX Mali-Optimized Vertex Shader
 * 
 * Optimizations:
 * - Packed output varyings
 * - IDVS (Index-Driven Vertex Shading) friendly
 * - Reduced varying count for bandwidth
 */

#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable

// Input attributes
layout(location = 0) in highp vec3 inPosition;
layout(location = 1) in mediump vec3 inNormal;
layout(location = 2) in mediump vec2 inTexCoord;
layout(location = 3) in mediump vec4 inColor;
layout(location = 4) in mediump vec4 inTangent;

// Packed outputs for Mali (reduces varying slots)
layout(location = 0) out mediump vec4 fragTexCoordAndZ;  // xy=uv, z=depth, w=spare
layout(location = 1) out mediump vec4 fragNormalAndSpec; // xyz=normal, w=specular intensity
layout(location = 2) out mediump vec4 fragColor;
layout(location = 3) out highp vec3 fragWorldPos;

// UBO
layout(set = 0, binding = 0, std140) uniform Matrices {
    highp mat4 model;
    highp mat4 view;
    highp mat4 projection;
    highp mat4 mvp;
    highp mat4 normalMatrix;
} m;

// Push constants
layout(push_constant) uniform PC {
    highp vec4 objectPos;
    mediump vec4 animParams;  // x=time, y=scale, zw=uvOffset
} pc;

invariant gl_Position;

void main() {
    // World position
    highp vec4 worldPos = m.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Clip position
    gl_Position = m.mvp * vec4(inPosition, 1.0);
    
    // Pack texture coords and depth
    fragTexCoordAndZ = vec4(
        inTexCoord + pc.animParams.zw,
        gl_Position.z / gl_Position.w,  // Linear depth
        1.0
    );
    
    // Pack normal and specular
    mediump vec3 worldNormal = normalize(mat3(m.normalMatrix) * inNormal);
    fragNormalAndSpec = vec4(worldNormal, 1.0);
    
    // Color passthrough
    fragColor = inColor;
}
