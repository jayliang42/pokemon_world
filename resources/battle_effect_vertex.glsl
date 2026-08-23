#version 410 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 local_position;
out vec3 view_position;
out vec3 view_normal;
out vec2 vertex_tex;

void main()
{
	mat4 modelView = V * M;
	vec4 viewPosition = modelView * vec4(vertPos, 1.0);
	local_position = vertPos;
	view_position = viewPosition.xyz;
	view_normal = normalize(mat3(modelView) * vertNor);
	vertex_tex = vertTex;
	gl_Position = P * viewPosition;
}
