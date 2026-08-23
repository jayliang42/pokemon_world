#version 330 core
out vec4 color;
in vec3 vertex_pos;
in vec3 view_pos;
in vec3 vertex_normal;
in vec2 vertex_tex;


uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec3 camoff;
uniform vec3 campos;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
vec2 texcoords=vertex_tex;
float t=1./100.;
texcoords -= vec2(camoff.x,camoff.z)*t;

vec3 heightColor = texture(tex, texcoords).rgb;
float terrainVariation = 0.22 + heightColor.r * 0.78;
vec3 baseColor = texture(tex2, texcoords * 50.0).rgb * terrainVariation;
vec3 normal = normalize(vertex_normal);
float diffuse = max(dot(normal, normalize(sunDirection)), 0.0);
float skyFill = 0.5 + 0.5 * normal.y;
vec3 illumination = ambientColor * (0.82 + 0.18 * skyFill) +
                    sunColor * diffuse * 0.72;
float fogAmount = smoothstep(fogStart, fogEnd, length(view_pos));
color.rgb = mix(baseColor * illumination, fogColor, fogAmount);
color.a=1.0;

float len = length(vertex_pos.xz+campos.xz);
len-=41.0;
len/=8.;
len=clamp(len, 0.0, 1.0);
color.a=1.0-len;


}
