#version 410 core
out vec4 color;

in vec3 local_position;
in vec3 view_position;
in vec3 view_normal;
in vec2 vertex_tex;

uniform float time;
uniform vec3 effectColor;
uniform vec3 coreColor;
uniform float opacity;
uniform float shellAmount;

void main()
{
	vec3 normal = normalize(view_normal);
	vec3 viewDirection = normalize(-view_position);
	float facing = max(dot(normal, viewDirection), 0.0);
	float rim = pow(1.0 - facing, 2.25);

	float flowingBands = 0.5 + 0.5 * sin(
		local_position.y * 17.0 + local_position.x * 7.0 - time * 12.0);
	float crossFlow = 0.5 + 0.5 * sin(
		(vertex_tex.x + vertex_tex.y) * 24.0 + time * 8.0);
	float energy = 0.72 + flowingBands * 0.20 + crossFlow * 0.08;

	vec3 emission = mix(coreColor, effectColor,
	                    clamp(rim * 0.82 + flowingBands * 0.22, 0.0, 1.0));
	emission *= energy + rim * 0.52;

	float orbAlpha = 0.62 + rim * 0.28 + flowingBands * 0.10;
	float shellAlpha = 0.08 + rim * 0.92;
	float alpha = mix(orbAlpha, shellAlpha, shellAmount) * opacity;
	if (alpha < 0.01)
	{
		discard;
	}
	color = vec4(emission, alpha);
}
