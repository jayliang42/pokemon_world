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

// Layer multi-directional bands into soft cloud clusters.  The extra y-axis
// variation breaks up vertical stripes while keeping the field horizon clear.
float cloudLowA = sin(skyDirection.x * 9.0 + skyDirection.z * 11.0 + skyDirection.y * 7.0);
float cloudLowB = sin(skyDirection.x * 15.0 - skyDirection.z * 5.0 + skyDirection.y * 13.0);
float cloudDetail = sin(skyDirection.x * 28.0 + skyDirection.z * 19.0 - skyDirection.y * 17.0);
float cloudNoise = 0.5 + 0.5 * (cloudLowA * cloudLowB * 0.68 + cloudDetail * 0.32);
float cloudHeight = smoothstep(0.02, 0.28, skyDirection.y) * smoothstep(0.98, 0.36, skyDirection.y);
float cloudMask = smoothstep(0.58, 0.74, cloudNoise) * cloudHeight;
skyColor = mix(skyColor, vec3(1.0, 0.98, 0.94), cloudMask * 0.78);

color = vec4(skyColor, 1.0);
}
