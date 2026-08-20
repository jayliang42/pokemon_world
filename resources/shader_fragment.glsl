#version 330 core
out vec4 color;
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform vec3 campos;

uniform sampler2D tex;
uniform sampler2D tex2;

void main()
{
vec3 skyDirection = normalize(vertex_pos);
float horizon = clamp((skyDirection.y + 0.18) / 0.9, 0.0, 1.0);
vec3 horizonColor = vec3(0.74, 0.91, 1.0);
vec3 zenithColor = vec3(0.12, 0.48, 0.94);
vec3 skyColor = mix(horizonColor, zenithColor, smoothstep(0.0, 1.0, horizon));

// Use the original panorama as a natural cloud layer, while retaining the
// procedural gradient at the horizon so the field still blends cleanly.
vec3 photoSky = texture(tex, vertex_tex).rgb;
float photoWeight = smoothstep(0.06, 0.38, skyDirection.y);
skyColor = mix(skyColor, photoSky, photoWeight * 0.82);

color = vec4(skyColor, 1.0);
}
