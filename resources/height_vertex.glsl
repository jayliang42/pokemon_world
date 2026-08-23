#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
out vec3 vertex_pos;
out vec3 view_pos;
out vec3 vertex_normal;
out vec2 vertex_tex;
uniform sampler2D tex;

uniform vec3 camoff;

float terrainHeight(vec2 coordinates)
{
	return texture(tex, coordinates).r * 10.0 - 5.0;
}

void main()
{
	vec2 texcoords=vertTex;
	float t=1./100.;
	texcoords -= vec2(camoff.x,camoff.z)*t;
	float height = terrainHeight(texcoords);
	float leftHeight = terrainHeight(texcoords - vec2(t, 0.0));
	float rightHeight = terrainHeight(texcoords + vec2(t, 0.0));
	float nearHeight = terrainHeight(texcoords - vec2(0.0, t));
	float farHeight = terrainHeight(texcoords + vec2(0.0, t));
	vec3 worldNormal = normalize(vec3(leftHeight - rightHeight, 2.0, nearHeight - farHeight));


	vec4 tpos =  vec4(vertPos, 1.0);
	tpos.z -=camoff.z;
	tpos.x -=camoff.x;
	tpos.y -=camoff.y;
	tpos =  M * tpos;


	tpos.y += height;
	vertex_pos = tpos.xyz;
	vec4 viewPosition = V * tpos;
	view_pos = viewPosition.xyz;
	vertex_normal = normalize(mat3(V) * worldNormal);
	gl_Position = P * viewPosition;
	vertex_tex = vertTex;
}
