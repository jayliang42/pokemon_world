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

float cloudLayerA = 0.5 + 0.5 * sin(skyDirection.x * 16.0 + skyDirection.z * 9.0);
float cloudLayerB = 0.5 + 0.5 * sin(skyDirection.x * 37.0 - skyDirection.z * 21.0 + skyDirection.y * 15.0);
float cloudNoise = cloudLayerA * 0.55 + cloudLayerB * 0.45;
float cloudMask = smoothstep(0.46, 0.64, cloudNoise) * smoothstep(0.2, 0.82, skyDirection.y);
skyColor = mix(skyColor, vec3(1.0), cloudMask * 0.72);

color = vec4(skyColor, 1.0);
}
