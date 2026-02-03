/**
 * RPCSX PowerVR-Optimized Vertex Shader
 * 
 * Optimizations:
 * - TBDR-friendly vertex processing
 * - Minimal varying usage
 * - Pre-transformed positions for ISP efficiency
 */

#version 450

// Inputs
layout(location = 0) in highp vec3 inPosition;
layout(location = 1) in mediump vec3 inNormal;
layout(location = 2) in mediump vec2 inTexCoord;
layout(location = 3) in mediump vec4 inColor;

// Outputs
layout(location = 0) out mediump vec2 fragTexCoord;
layout(location = 1) out mediump vec3 fragNormal;
layout(location = 2) out mediump vec4 fragColor;
layout(location = 3) out highp vec3 fragWorldPos;

// Matrices UBO
layout(set = 0, binding = 0, std140) uniform Matrices {
    highp mat4 model;
    highp mat4 view;
    highp mat4 projection;
    highp mat4 mvp;
    highp mat3 normalMat;
} mat;

// Push constants
layout(push_constant) uniform Constants {
    highp vec4 objectPos;
    mediump vec2 uvOffset;
    mediump vec2 uvScale;
} pc;

invariant gl_Position;

void main() {
    // World position for lighting
    highp vec4 worldPos = mat.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Clip space position
    gl_Position = mat.mvp * vec4(inPosition, 1.0);
    
    // Transform normal to world space
    fragNormal = mat.normalMat * inNormal;
    
    // Texture coordinates with animation
    fragTexCoord = inTexCoord * pc.uvScale + pc.uvOffset;
    
    // Pass through vertex color
    fragColor = inColor;
}
