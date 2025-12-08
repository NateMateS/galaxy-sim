#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gasTexture;          // Low-Res Color
uniform sampler2D quarterResLinearDepth;  // Low-Res Depth (Linear)
uniform sampler2DMS highResDepth;      // High-Res Depth (MSAA)

uniform float zNear;
uniform float zFar;
uniform float depthSensitivity;

float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * zNear * zFar) / (zFar + zNear - z * (zFar - zNear));
}

void main()
{
    // 1. Get High-Res Depth at current pixel
    ivec2 screenCoords = ivec2(gl_FragCoord.xy);
    float dHigh = LinearizeDepth(texelFetch(highResDepth, screenCoords, 0).r);

    // 2. Bilateral Upsample
    // We look at the 4 nearest low-res pixels (bilinear neighborhood)
    ivec2 sizeLow = textureSize(gasTexture, 0);
    vec2 posLow = TexCoords * vec2(sizeLow) - vec2(0.5);
    ivec2 basePos = ivec2(floor(posLow));
    vec2 f = fract(posLow); // Fractional offset

    // Neighbors: TL, TR, BL, BR
    ivec2 off[4] = ivec2[](
        ivec2(0,0), ivec2(1,0),
        ivec2(0,1), ivec2(1,1)
    );

    vec4 totalColor = vec4(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < 4; i++) {
        ivec2 p = basePos + off[i];

        // Fetch Low-Res Color
        vec4 colorLow = texelFetch(gasTexture, p, 0);

        // Fetch Low-Res Depth
        float dLow = texelFetch(quarterResLinearDepth, p, 0).r;

        // Spatial Weight (Bilinear)
        float wSpatial = (i == 0) ? (1.0 - f.x) * (1.0 - f.y) :
                         (i == 1) ? f.x * (1.0 - f.y) :
                         (i == 2) ? (1.0 - f.x) * f.y :
                                    f.x * f.y;

        // Depth Weight (Gaussian falloff based on depth difference)
        float depthDiff = abs(dHigh - dLow);
        // Sensitive to depth scale.
        float wDepth = 1.0 / (1.0 + depthDiff * depthDiff * depthSensitivity);

        float w = wSpatial * wDepth;

        totalColor += colorLow * w;
        totalWeight += w;
    }

    if (totalWeight > 0.0) {
        FragColor = totalColor / totalWeight;
    } else {
        // Fallback to bilinear if weights fail (e.g. extremely far depth diffs everywhere)
        FragColor = texture(gasTexture, TexCoords);
    }
}
