#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aBrightness;
layout (location = 3) in float aRadius;
layout (location = 4) in float aAngle;
layout (location = 5) in float aVelocity;

out vec3 Color;

uniform float screenHeight;

layout (std140) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 viewPos;
    float time;
};

void main() {
    // 1. ORBITAL MECHANICS
    float newAngle = aAngle + aVelocity * time;
    float cosA = cos(newAngle);
    float sinA = sin(newAngle);

    float newX = aRadius * cosA;
    float newZ = aRadius * sinA;
    vec3 newPos = vec3(newX, aPos.y, newZ);

    // 2. DOPPLER SHIFT CALCULATION
    // Calculate the instantaneous velocity vector of the star in World Space
    // v = r * omega (tangential to orbit)
    // vector direction is (-sin, 0, cos)
    vec3 orbitVelocity = vec3(-newZ, 0.0, newX) * aVelocity;
    // Note: The above line is equivalent to vec3(-sinA, 0, cosA) * r * v
    // because newZ = r*sinA, newX = r*cosA.

    // Transform velocity into View Space to find the component towards/away from camera
    // We only care about the direction, so 0.0 for w component
    vec3 viewSpaceVelocity = vec3(view * vec4(orbitVelocity, 0.0));

    // Z component in View Space:
    // Standard OpenGL Camera looks down -Z.
    // Objects moving towards camera have positive Z velocity (relative to camera).
    // Objects moving away have negative Z velocity.
    // Let's tune visually: 0.1 is an exaggeration factor.
    float radialSpeed = viewSpaceVelocity.z;
    float shift = radialSpeed * 0.1;
    // Clamp shift to prevent color inversion in extreme scenarios
    shift = clamp(shift, -0.9, 0.9);

    // Apply Doppler Tint
    // Redshift (Receding): Boost Red, Cut Blue
    // Blueshift (Approaching): Boost Blue, Cut Red
    // We dampen Green slightly to prevent de-saturation.
    // Note: Receding is negative Z velocity (negative shift), so we subtract shift to boost Red (1.0 - (-s) = 1.0 + s).
    vec3 dopplerColor = aColor * vec3(1.0 - shift, 1.0 - abs(shift) * 0.2, 1.0 + shift);

    gl_Position = projection * view * vec4(newPos, 1.0);

    // 3. ATTENUATION & TONE MAPPING
    float dist = gl_Position.w;
    if (dist < 0.1) dist = 0.1;

    float distanceFactor = 1.0 + 0.0001 * dist + 0.000005 * dist * dist;
    float attenuation = 1.0 / distanceFactor;
    float rawFlux = aBrightness * attenuation;

    // Gamma Compression (Lift Shadows)
    // Lifts faint stars to visibility while compressing bright core stars.
    float mappedBrightness = sqrt(rawFlux);

    // Exposure
    // Increased to 0.12 because the new Dual-Lobe shader concentrates energy
    // into a smaller core, so we need more brightness to make the glow visible.
    float exposure = 0.12;
    Color = dopplerColor * mappedBrightness * exposure;

    // 4. SIZE CALCULATION
    float screenScale = screenHeight / 1080.0;
    // Logarithmic bloom size
    // Base size 2.5 ensures everything is resolvable.
    float bloomSize = 2.5 + 3.0 * log(1.0 + mappedBrightness * 8.0);

    // Clamp to reasonable limits
    gl_PointSize = clamp(bloomSize * screenScale, 2.0 * screenScale, 12.0 * screenScale);
}
