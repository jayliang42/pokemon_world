#version 330 core
out vec4 color;
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform sampler2D tex;
uniform sampler2D tex2;
uniform float time;

void main()
{
vec3 skyDirection = normalize(vertex_pos);
float horizon = clamp((skyDirection.y + 0.18) / 0.9, 0.0, 1.0);
vec3 horizonColor = vec3(0.74, 0.91, 1.0);
vec3 zenithColor = vec3(0.12, 0.48, 0.94);
vec3 skyColor = mix(horizonColor, zenithColor, smoothstep(0.0, 1.0, horizon));

// Rotate the cloud sampling direction very slowly around the world. The sky
// sphere itself stays fixed, so turning the player preserves a stable horizon.
float cloudAngle = time * 0.018;
float cloudCos = cos(cloudAngle);
float cloudSin = sin(cloudAngle);
vec3 cloudDirection = vec3(
	cloudCos * skyDirection.x - cloudSin * skyDirection.z,
	skyDirection.y,
	cloudSin * skyDirection.x + cloudCos * skyDirection.z);
float cloudLowA = sin(cloudDirection.x * 9.0 + cloudDirection.z * 11.0 + cloudDirection.y * 7.0);
float cloudLowB = sin(cloudDirection.x * 15.0 - cloudDirection.z * 5.0 + cloudDirection.y * 13.0);
float cloudDetail = sin(cloudDirection.x * 28.0 + cloudDirection.z * 19.0 - cloudDirection.y * 17.0);
float cloudNoise = 0.5 + 0.5 * (cloudLowA * cloudLowB * 0.68 + cloudDetail * 0.32);
float cloudHeight = smoothstep(0.02, 0.28, skyDirection.y) * smoothstep(0.98, 0.36, skyDirection.y);
float cloudMask = smoothstep(0.58, 0.74, cloudNoise) * cloudHeight;
skyColor = mix(skyColor, vec3(1.0, 0.98, 0.94), cloudMask * 0.78);

vec3 sunDirection = normalize(vec3(-0.38, 0.82, -0.42));
float sunAlignment = max(dot(skyDirection, sunDirection), 0.0);
float sunHalo = pow(sunAlignment, 18.0) * 0.18 + pow(sunAlignment, 96.0) * 0.42;
float sunDisc = smoothstep(0.9988, 0.99965, sunAlignment);
skyColor += vec3(1.0, 0.78, 0.42) * sunHalo;
skyColor = mix(skyColor, vec3(1.0, 0.94, 0.76), sunDisc);

color = vec4(skyColor, 1.0);
}
