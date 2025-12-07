#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D srcTexture;
uniform vec2 srcResolution;

void main()
{
    vec2 srcTexelSize = 1.0 / srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Simple 5-tap Downsample (Center + 4 corners)
    // This leverages hardware bilinear filtering for the samples in between
    vec3 s0 = texture(srcTexture, TexCoords).rgb;
    vec3 s1 = texture(srcTexture, TexCoords + vec2(2.0*x, 2.0*y)).rgb;
    vec3 s2 = texture(srcTexture, TexCoords + vec2(-2.0*x, 2.0*y)).rgb;
    vec3 s3 = texture(srcTexture, TexCoords + vec2(2.0*x, -2.0*y)).rgb;
    vec3 s4 = texture(srcTexture, TexCoords + vec2(-2.0*x, -2.0*y)).rgb;

    FragColor = vec4((s0*4.0 + s1 + s2 + s3 + s4) / 8.0, 1.0);
}
