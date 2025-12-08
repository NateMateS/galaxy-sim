#version 430 core
out float LinearDepth;

in vec2 TexCoords;

uniform sampler2D depthMap; // Single-Sample Resolved Depth
uniform float zNear;
uniform float zFar;
uniform float downsampleScale; // e.g. 4.0 for Quarter-Res

// Linearize standard depth buffer value
float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * zNear * zFar) / (zFar + zNear - z * (zFar - zNear));
}

void main()
{
    // Calculate the base coordinate in the source (High-Res) texture
    // gl_FragCoord.xy gives pixel center in low-res (e.g., 0.5, 1.5)
    // We want the top-left corner of the corresponding block in high-res.
    ivec2 sourceBase = ivec2(floor(gl_FragCoord.xy) * downsampleScale);

    // Get source texture dimensions for bounds checking
    ivec2 texSize = textureSize(depthMap, 0);

    // Initialize minDepth to the farthest possible value (1.0)
    // We want to find the NEAREST depth in the block to preserve occlusion.
    float minDepth = 1.0;

    // Loop over the pixel block (e.g. 4x4)
    int scale = int(downsampleScale);
    for (int y = 0; y < scale; ++y)
    {
        for (int x = 0; x < scale; ++x)
        {
            ivec2 sampleCoord = sourceBase + ivec2(x, y);

            // Clamp coordinates to valid range to prevent undefined behavior
            sampleCoord = min(sampleCoord, texSize - ivec2(1));

            // Fetch depth from Texture (Single Sample)
            float depth = texelFetch(depthMap, sampleCoord, 0).r;

            // Take the minimum depth (closest to camera)
            minDepth = min(minDepth, depth);
        }
    }

    // Write out the linear depth directly to the R32F texture
    LinearDepth = LinearizeDepth(minDepth);
}
