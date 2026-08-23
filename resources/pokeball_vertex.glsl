#version 410 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 vertex_pos;
out vec3 vertex_normal;
out vec3 local_position;

void main()
{
	mat4 modelView = V * M;
	vec4 viewPosition = modelView * vec4(vertPos, 1.0);
	vertex_pos = viewPosition.xyz;
	vertex_normal = normalize(mat3(modelView) * vertNor);
	local_position = vertPos;
	gl_Position = P * viewPosition;
}
