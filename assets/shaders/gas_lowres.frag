#version 330 core
out vec4 FragColor;

in vec4 Color;
in float LinearDepth;

// Low-Res Linear Depth Texture (R32F)
// No MSAA overhead, no linearization math in pixel shader
uniform sampler2D quarterResLinearDepth;

uniform float softnessScale;

void main()
{
    // 1. Radial Softness (Particle Shape)
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if (distSq > 0.25) discard;

    // 2. Depth Buffer Softness
    // Read directly from the linear depth texture
    // gl_FragCoord is already in quarter-res space (0..w/4, 0..h/4)
    float sceneDepthLinear = texelFetch(quarterResLinearDepth, ivec2(gl_FragCoord.xy), 0).r;

    float depthDelta = sceneDepthLinear - LinearDepth;

    if (depthDelta <= 0.0) discard;

    float alphaSoft = clamp(depthDelta * softnessScale, 0.0, 1.0);
    float alphaRadial = exp(-distSq * 16.0);

    float finalAlpha = Color.a * alphaRadial * alphaSoft;

    if (finalAlpha < 0.005) discard;

    FragColor = vec4(Color.rgb, finalAlpha);
}
