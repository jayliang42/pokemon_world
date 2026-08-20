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

// Layer two low-frequency bands into recognizable cloud puffs.  Keeping the
// mask away from the horizon preserves the open field silhouette below.
float cloudBand = 0.5 + 0.5 * sin(skyDirection.x * 10.0 + skyDirection.z * 5.0);
float cloudPuffs = 0.5 + 0.5 * sin(skyDirection.x * 5.0 - skyDirection.z * 9.0);
float cloudDetail = 0.5 + 0.5 * sin(skyDirection.x * 25.0 - skyDirection.z * 18.0);
float cloudNoise = cloudBand * 0.45 + cloudPuffs * 0.4 + cloudDetail * 0.15;
float cloudHeight = smoothstep(0.02, 0.3, skyDirection.y) * smoothstep(0.96, 0.42, skyDirection.y);
float cloudMask = smoothstep(0.52, 0.66, cloudNoise) * cloudHeight;
skyColor = mix(skyColor, vec3(1.0, 0.98, 0.94), cloudMask * 0.9);

color = vec4(skyColor, 1.0);
}
