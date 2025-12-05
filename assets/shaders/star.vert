#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aBrightness;
layout (location = 3) in float aRadius;
layout (location = 4) in float aAngle;
layout (location = 5) in float aVelocity;

out vec3 Color;

uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main() {
    // Calculate new position based on orbital mechanics
    float newAngle = aAngle + aVelocity * time;
    float newX = aRadius * cos(newAngle);
    float newZ = aRadius * sin(newAngle);

    // Y remains the same (disk height)
    vec3 newPos = vec3(newX, aPos.y, newZ);

    gl_Position = projection * view * vec4(newPos, 1.0);
    Color = aColor * aBrightness;

    // Point size attenuation
    // Determine distance from camera (w component of clip space is -z in view space)
    // But we can also just take length of gl_Position before perspective division if needed,
    // or clearer: just use the w component which is roughly depth.
    // A simple inverse distance formula works well.

    float dist = gl_Position.w;
    if (dist < 0.1) dist = 0.1;

    // Scale factor 3000.0 is arbitrary, tune for taste
    gl_PointSize = clamp(2000.0 / dist, 1.0, 5.0);
}
