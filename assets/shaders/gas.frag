#version 330 core
out vec4 FragColor;

in vec4 Color;
in float LinearDepth;

uniform sampler2DMS depthMap; // Multisampled Depth
uniform float zNear;
uniform float zFar;
uniform float softnessScale; // Controls how soft the intersection is (e.g. 1.0)

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * zNear * zFar) / (zFar + zNear - z * (zFar - zNear));
}

void main()
{
    // 1. Radial Softness (Particle Shape)
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if (distSq > 0.25) discard;

    // Smooth radial falloff (Gaussian-ish)
    float alphaRadial = exp(-distSq * 16.0);

    // 2. Depth Buffer Softness (Intersection with geometry)
    // Use texelFetch for MSAA texture (Sample 0 is sufficient for soft particles)
    ivec2 screenCoords = ivec2(gl_FragCoord.xy);
    float sceneDepthNonLinear = texelFetch(depthMap, screenCoords, 0).r;
    float sceneDepthLinear = LinearizeDepth(sceneDepthNonLinear);

    // Calculate difference between scene geometry and this particle
    float depthDelta = sceneDepthLinear - LinearDepth;

    // Fade out as the particle approaches geometry behind it
    // Clamp to [0, 1]
    float alphaSoft = clamp(depthDelta * softnessScale, 0.0, 1.0);

    // Final Alpha
    float finalAlpha = Color.a * alphaRadial * alphaSoft;

    FragColor = vec4(Color.rgb, finalAlpha);
}
