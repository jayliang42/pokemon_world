#version 410 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float surfaceDeform;
uniform float animationMode;
uniform float wingAngle;
uniform float tailAngle;
uniform float breathingScale;
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
	vec3 localNormal = normalize(vertNor + surfaceDeform * vec3(broadNoise, verticalNoise, ridgeNoise));
	if (animationMode > 0.5)
	{
		float bodyWeight = (1.0 - smoothstep(0.34, 0.68, abs(localPosition.x))) *
		                   (1.0 - smoothstep(0.62, 0.92, abs(localPosition.z)));
		localPosition.y *= mix(1.0, breathingScale, bodyWeight);

		float wingWeight = smoothstep(0.16, 0.58, abs(localPosition.x)) *
		                   smoothstep(-0.22, 0.12, localPosition.y) *
		                   smoothstep(-0.08, 0.24, localPosition.z);
		float side = localPosition.x < 0.0 ? -1.0 : 1.0;
		float wingRootX = side * 0.13;
		float wingRotation = wingAngle * side;
		float wingCos = cos(wingRotation);
		float wingSin = sin(wingRotation);
		vec2 wingOffset = vec2(localPosition.x - wingRootX, localPosition.y - 0.02);
		vec2 wingPosition = vec2(
			wingCos * wingOffset.x - wingSin * wingOffset.y,
			wingSin * wingOffset.x + wingCos * wingOffset.y);
		vec3 flappedPosition = localPosition;
		flappedPosition.xy = wingPosition + vec2(wingRootX, 0.02);
		localPosition = mix(localPosition, flappedPosition, wingWeight);
		vec2 wingNormal = vec2(
			wingCos * localNormal.x - wingSin * localNormal.y,
			wingSin * localNormal.x + wingCos * localNormal.y);
		localNormal.xy = mix(localNormal.xy, wingNormal, wingWeight);

		float tailWeight = smoothstep(0.28, 0.88, -localPosition.z) *
		                   (1.0 - smoothstep(0.06, 0.34, localPosition.y));
		float tailRotation = tailAngle * tailWeight;
		float tailCos = cos(tailRotation);
		float tailSin = sin(tailRotation);
		vec2 tailOffset = vec2(localPosition.x, localPosition.z + 0.24);
		localPosition.x = tailCos * tailOffset.x + tailSin * tailOffset.y;
		localPosition.z = -tailSin * tailOffset.x + tailCos * tailOffset.y - 0.24;
		vec2 tailNormal = vec2(
			tailCos * localNormal.x + tailSin * localNormal.z,
			-tailSin * localNormal.x + tailCos * localNormal.z);
		localNormal.x = tailNormal.x;
		localNormal.z = tailNormal.y;
	}
	vec4 viewPosition = modelView * vec4(localPosition, 1.0);
	vertex_normal = normalize(mat3(modelView) * localNormal);
	vertex_pos = viewPosition.xyz;
	gl_Position = P * viewPosition;
	vertex_tex = vertTex;
}
