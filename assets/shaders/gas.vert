#version 430 core

// Inputs match the VAO layout defined in GalacticGas.cpp
layout (location = 0) in vec3 aViewPos;    // From GasRender.position_depth.xyz
layout (location = 1) in vec4 aColor;      // From GasRender.color
layout (location = 2) in float aLinearDepth; // From GasRender.position_depth.w
layout (location = 3) in float aSize;      // From GasRender.size

out vec4 Color;
out float LinearDepth;

layout (std140) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 viewPos;
    float time;
};

uniform float pointMultiplier;

void main()
{
    // The position is already in View Space (from Compute Shader)
    gl_Position = projection * vec4(aViewPos, 1.0);

    // Pass data to Fragment Shader
    Color = aColor;
    LinearDepth = aLinearDepth;

    // Point Size
    // This is the size ready for rasterization.
    gl_PointSize = aSize * pointMultiplier;
}
