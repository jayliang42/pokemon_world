#version 330 core
out vec4 color;
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform sampler2D tex;
uniform sampler2D tex2;
uniform float time;
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float daylight;

float hash31(vec3 position)
{
	position = fract(position * 0.1031);
	position += dot(position, position.yzx + 33.33);
	return fract((position.x + position.y) * position.z);
}

float valueNoise(vec3 position)
{
	vec3 cell = floor(position);
	vec3 local = fract(position);
	vec3 blend = local * local * (3.0 - 2.0 * local);

	float x00 = mix(hash31(cell + vec3(0.0, 0.0, 0.0)),
	                  hash31(cell + vec3(1.0, 0.0, 0.0)), blend.x);
	float x10 = mix(hash31(cell + vec3(0.0, 1.0, 0.0)),
	                  hash31(cell + vec3(1.0, 1.0, 0.0)), blend.x);
	float x01 = mix(hash31(cell + vec3(0.0, 0.0, 1.0)),
	                  hash31(cell + vec3(1.0, 0.0, 1.0)), blend.x);
	float x11 = mix(hash31(cell + vec3(0.0, 1.0, 1.0)),
	                  hash31(cell + vec3(1.0, 1.0, 1.0)), blend.x);
	float y0 = mix(x00, x10, blend.y);
	float y1 = mix(x01, x11, blend.y);
	return mix(y0, y1, blend.z);
}

float cloudFbm(vec3 position)
{
	float value = 0.0;
	float amplitude = 0.5;
	for (int octave = 0; octave < 4; ++octave)
	{
		value += valueNoise(position) * amplitude;
		position = position * 2.03 + vec3(17.1, 9.2, 13.7);
		amplitude *= 0.5;
	}
	return value;
}

void main()
{
vec3 skyDirection = normalize(vertex_pos);
float horizon = clamp((skyDirection.y + 0.18) / 0.9, 0.0, 1.0);
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
float cloudBase = cloudFbm(cloudDirection * 3.4 + vec3(2.3, 1.1, -3.7));
float cloudDetail = cloudFbm(cloudDirection * 7.6 + vec3(-4.1, 3.8, 1.6));
float cloudNoise = cloudBase * 0.76 + cloudDetail * 0.24;
float cloudHeight = smoothstep(0.02, 0.20, skyDirection.y) *
	                (1.0 - smoothstep(0.58, 0.96, skyDirection.y));
float cloudMask = smoothstep(0.50, 0.66, cloudNoise) * cloudHeight;
vec3 cloudColor = mix(vec3(0.18, 0.23, 0.34), vec3(1.0, 0.985, 0.95), daylight);
skyColor = mix(skyColor, cloudColor, cloudMask * 0.86);

float sunAlignment = max(dot(skyDirection, sunDirection), 0.0);
float sunHalo = pow(sunAlignment, 18.0) * 0.18 + pow(sunAlignment, 96.0) * 0.42;
float sunDisc = smoothstep(0.9988, 0.99965, sunAlignment);
skyColor += sunColor * sunHalo;
skyColor = mix(skyColor, sunColor, sunDisc);

color = vec4(skyColor, 1.0);
}
