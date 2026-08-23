#version 410 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
out vec3 vertex_pos;
out vec3 vertex_normal;
out vec2 vertex_tex;
void main()
{
	mat4 modelView = V * M;
	vec4 viewPosition = modelView * vec4(vertPos, 1.0);
	vertex_normal = normalize(mat3(modelView) * vertNor);
	vertex_pos = viewPosition.xyz;
	gl_Position = P * viewPosition;
	vertex_tex = vertTex;
}
