#version 410 core
out vec4 color;

in vec3 vertex_pos;
in vec3 vertex_normal;
in vec3 local_position;
in vec2 vertex_tex;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
	vec3 local = normalize(local_position);
	vec3 shellColor = local.y > 0.0
		? vec3(0.86, 0.035, 0.045)
		: vec3(0.92, 0.93, 0.94);

	float belt = 1.0 - smoothstep(0.045, 0.105, abs(local.y));
	vec3 baseColor = mix(shellColor, vec3(0.025, 0.028, 0.035), belt);

	float front = smoothstep(0.64, 0.78, local.z);
	float buttonRadius = length(local.xy);
	float buttonRim = (1.0 - smoothstep(0.22, 0.27, buttonRadius)) * front;
	float buttonFace = (1.0 - smoothstep(0.125, 0.17, buttonRadius)) * front;
	baseColor = mix(baseColor, vec3(0.018, 0.020, 0.025), buttonRim);
	baseColor = mix(baseColor, vec3(0.82, 0.86, 0.90), buttonFace);
	baseColor *= 0.995 + vertex_tex.y * 0.005;

	vec3 normal = normalize(vertex_normal);
	float diffuse = max(dot(normal, normalize(sunDirection)), 0.0);
	vec3 viewDirection = normalize(-vertex_pos);
	float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.6);
	float gloss = pow(max(dot(reflect(-normalize(sunDirection), normal), viewDirection), 0.0), 26.0);
	vec3 illumination = ambientColor * 0.82 + sunColor * diffuse * 0.76;
	vec3 litColor = baseColor * illumination + sunColor * (rim * 0.09 + gloss * 0.34);
	float fogAmount = smoothstep(fogStart, fogEnd, length(vertex_pos));
	color = vec4(mix(litColor, fogColor, fogAmount), 1.0);
}
