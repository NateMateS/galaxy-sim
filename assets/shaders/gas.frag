#version 330 core
out vec4 FragColor;

in vec4 Color;
in float LinearDepth;

uniform sampler2DMS depthMap; // Multisampled Depth
uniform float zNear;
uniform float zFar;
uniform float softnessScale; // Controls how soft the intersection is (e.g. 1.0)
uniform float resolutionScale; // 1.0 for full-res, 4.0 for quarter-res (to scale gl_FragCoord)

void main()
{
    // 1. Radial Softness (Particle Shape)
    // Early discard for circle shape
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if (distSq > 0.25) discard;

    // 2. Depth Buffer Softness (Intersection with geometry)
    // Use texelFetch for MSAA texture (Sample 0 is sufficient for soft particles)
    // Adjust coordinates if rendering at quarter resolution
    ivec2 screenCoords = ivec2(gl_FragCoord.xy * resolutionScale);
    float sceneDepthNonLinear = texelFetch(depthMap, screenCoords, 0).r;

    // Optimized LinearizeDepth
    // Derived from projection matrix linearization
    float sceneDepthLinear = (zNear * zFar) / (zFar - sceneDepthNonLinear * (zFar - zNear));

    // Calculate difference between scene geometry and this particle
    // If sceneDepth < LinearDepth (particle is behind geometry), depthDelta is negative
    float depthDelta = sceneDepthLinear - LinearDepth;

    // Early discard if particle is behind geometry
    if (depthDelta <= 0.0) discard;

    // Fade out as the particle approaches geometry behind it
    float alphaSoft = clamp(depthDelta * softnessScale, 0.0, 1.0);

    // 3. Smooth radial falloff (Gaussian-ish)
    float alphaRadial = exp(-distSq * 16.0);

    // Final Alpha
    float finalAlpha = Color.a * alphaRadial * alphaSoft;

    // Optimization: Don't blend nearly invisible pixels
    if (finalAlpha < 0.005) discard;

    FragColor = vec4(Color.rgb, finalAlpha);
}
