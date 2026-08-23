#version 410 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float surfaceDeform;
out vec3 vertex_pos;
out vec3 vertex_normal;
out vec2 vertex_tex;

void main()
{
	mat4 modelView = V * M;
	float broadNoise = sin(vertPos.x * 4.7 + vertPos.y * 2.3 + vertPos.z * 4.1) * 0.55;
	float ridgeNoise = sin(vertPos.x * 9.7 - vertPos.z * 7.1) * 0.25;
	float verticalNoise = cos(vertPos.y * 8.3 + vertPos.z * 3.1) * 0.20;
	float deformation = 1.0 + surfaceDeform * (broadNoise + ridgeNoise + verticalNoise);
	vec3 localPosition = vertPos * deformation;
	vec3 roughNormal = normalize(vertNor + surfaceDeform * vec3(broadNoise, verticalNoise, ridgeNoise));
	vec4 viewPosition = modelView * vec4(localPosition, 1.0);
	vertex_normal = normalize(mat3(modelView) * roughNormal);
	vertex_pos = viewPosition.xyz;
	gl_Position = P * viewPosition;
	vertex_tex = vertTex;
}
