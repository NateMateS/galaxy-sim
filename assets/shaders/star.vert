#version 430 core
layout(location = 0) in vec4 aPosDoppler; // xyz: pos, w: doppler
layout(location = 1) in vec4 aColor;      // Unpacked from uint
layout(location = 2) in float aSize;      // size/brightness

out vec4 vColor; // rgb: color, a: brightness

layout(std140, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 viewPosTime;
};

uniform float screenHeight;

void main() {
    vec3 pos = aPosDoppler.xyz;
    float doppler = aPosDoppler.w;
    vec3 color = aColor.rgb;
    float brightness = aSize;

    gl_Position = projection * view * vec4(pos, 1.0);

    // Size calculation
    float screenScale = screenHeight / 1080.0;
    // Base size 2.5 ensures everything is resolvable.
    // Brightness here is already "mappedBrightness" from compute shader
    float bloomSize = 2.5 + 3.0 * log(1.0 + brightness * 8.0);

    gl_PointSize = clamp(bloomSize * screenScale, 2.0 * screenScale, 12.0 * screenScale);

    // Pass color and brightness (in alpha channel) to fragment shader
    vColor = vec4(color, brightness);
}
