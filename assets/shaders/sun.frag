#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D sunTexture;
uniform float time;
uniform vec3 viewPos;

void main()
{
    // 1. Animated Surface
    // Distort UVs with time
    float speed = 0.05;
    vec2 uv1 = TexCoords + vec2(time * speed, time * speed * 0.5);
    vec2 uv2 = TexCoords - vec2(time * speed * 0.8, time * speed * 0.2);

    vec3 col1 = texture(sunTexture, uv1).rgb;
    vec3 col2 = texture(sunTexture, uv2).rgb;

    // Blend them for turbulence
    vec3 surfaceColor = mix(col1, col2, 0.5);

    // Boost brightness for HDR bloom
    surfaceColor *= 2.5;

    // 2. Fresnel Glow (Corona)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 norm = normalize(Normal);
    float fresnel = 1.0 - max(dot(viewDir, norm), 0.0);
    fresnel = pow(fresnel, 2.0);

    vec3 coronaColor = vec3(1.0, 0.6, 0.2) * 4.0; // Super bright orange

    vec3 result = mix(surfaceColor, coronaColor, fresnel);

    FragColor = vec4(result, 1.0);
}
