#version 410 core
out vec4 color;
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
	vec3 normal = normalize(vertex_normal);
	vec3 baseColor = texture(tex, vertex_tex).rgb;
	float diffuse = max(dot(normal, normalize(sunDirection)), 0.0);
	float skyFill = 0.5 + 0.5 * normal.y;
	vec3 illumination = ambientColor * (0.78 + 0.22 * skyFill) +
	                    sunColor * diffuse * 0.82;
	vec3 viewDirection = normalize(-vertex_pos);
	float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.5);
	vec3 litColor = baseColor * illumination + sunColor * rim * 0.10;
	float fogAmount = smoothstep(fogStart, fogEnd, length(vertex_pos));
	color = vec4(mix(litColor, fogColor, fogAmount), 1.0);
}
