#version 330 core
out vec4 color;
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform vec3 campos;

uniform sampler2D tex;
uniform sampler2D tex2;

void main()
{
vec3 skyDirection = normalize(vertex_pos);
float horizon = clamp((skyDirection.y + 0.18) / 0.9, 0.0, 1.0);
vec3 horizonColor = vec3(0.74, 0.91, 1.0);
vec3 zenithColor = vec3(0.12, 0.48, 0.94);
vec3 skyColor = mix(horizonColor, zenithColor, smoothstep(0.0, 1.0, horizon));

float cloudLayerA = 0.5 + 0.5 * sin(skyDirection.x * 10.0 + skyDirection.z * 6.0);
float cloudLayerB = 0.5 + 0.5 * sin(skyDirection.x * 25.0 - skyDirection.z * 17.0 + skyDirection.y * 13.0);
float cloudNoise = cloudLayerA * 0.6 + cloudLayerB * 0.4;
float cloudMask = smoothstep(0.62, 0.82, cloudNoise) * smoothstep(0.08, 0.62, skyDirection.y);
skyColor = mix(skyColor, vec3(1.0), cloudMask * 0.5);

color = vec4(skyColor, 1.0);
}
