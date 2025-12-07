#version 330 core

// 1. Orbital Mechanics (vec3: radius, angle, velocity)
layout (location = 0) in vec3 aOrbital;
// 2. Local Shape (vec3: x, y, z offsets from cloud center)
layout (location = 1) in vec3 aOffset;
// 3. Visuals (vec4: r, g, b, a)
layout (location = 2) in vec4 aColor;
// 4. Animation Params (vec3: size, turbulencePhase, turbulenceSpeed)
layout (location = 3) in vec3 aParams;

out vec4 Color;
out float LinearDepth;

uniform mat4 view;
uniform mat4 projection;
uniform float u_Time;
uniform float pointScale;

void main()
{
    float orbitalRadius = aOrbital.x;
    float initialAngle = aOrbital.y;
    float angularVelocity = aOrbital.z;

    // 1. Calculate Current Orbital Angle
    float currentAngle = initialAngle + angularVelocity * u_Time;

    // 2. Calculate Cloud Center Position
    float cosA = cos(currentAngle);
    float sinA = sin(currentAngle);

    vec3 cloudCenter = vec3(orbitalRadius * cosA, 0.0, orbitalRadius * sinA);

    // 3. Rotate the Cloud's Shape (Local Offset) to match Galaxy Rotation
    // This ensures the cloud doesn't just translate, but rotates with the galaxy
    float localX = aOffset.x * cosA - aOffset.z * sinA;
    float localZ = aOffset.x * sinA + aOffset.z * cosA;
    vec3 rotatedOffset = vec3(localX, aOffset.y, localZ);

    // 4. Apply Turbulence (Sine wave animation)
    float turbulence = sin(u_Time * aParams.z + aParams.y);
    // Apply turbulence mainly to Y (up/down) and slightly to radius
    vec3 turbOffset = vec3(0.0, turbulence * 2.0, 0.0);

    // 5. Final World Position
    vec3 worldPos = cloudCenter + rotatedOffset + turbOffset;

    // 6. View Space Position
    vec4 viewPos = view * vec4(worldPos, 1.0);
    gl_Position = projection * viewPos;

    // 7. Point Size Attenuation with Safety Checks
    float dist = length(viewPos.xyz);

    // Safety: Prevent DivByZero or Infinite/NaN size if particle hits camera
    if (dist < 0.1) dist = 0.1;

    float baseSize = aParams.x;
    float scale = (pointScale < 1.0) ? 400.0 : pointScale;

    float finalSize = scale * baseSize * (1.0 / dist);

    // Safety: Clamp to reasonable hardware limits
    gl_PointSize = clamp(finalSize, 1.0, 256.0);

    // 8. Output Color
    Color = aColor;

    // Fade out particles that are too close to camera to prevent popping
    float alphaFade = smoothstep(1.0, 50.0, dist);
    Color.a *= alphaFade;

    // 9. Output Linear Depth for Soft Particles
    LinearDepth = -viewPos.z;
}
