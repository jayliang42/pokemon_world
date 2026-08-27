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
uniform vec3 meadowTerrainTint;
uniform vec2 moonRegionCenter;
uniform vec2 moonRegionRadii;
uniform vec3 moonTerrainTint;
uniform vec2 redRegionCenter;
uniform vec2 redRegionRadii;
uniform vec3 redTerrainTint;
uniform vec4 trailSegments[9];
uniform float trailHalfWidths[9];

float regionInfluence(vec2 worldPosition, vec2 center, vec2 radii)
{
	float width = max(radii.y - radii.x, 0.001);
	float progress = clamp((distance(worldPosition, center) - radii.x) / width,
	                       0.0, 1.0);
	return 1.0 - progress * progress * (3.0 - 2.0 * progress);
}

float trailInfluence(vec2 worldPosition, vec4 segment, float halfWidth)
{
	vec2 start = segment.xy;
	vec2 delta = segment.zw - start;
	float lengthSquared = max(dot(delta, delta), 0.0001);
	float progress = clamp(dot(worldPosition - start, delta) / lengthSquared,
	                       0.0, 1.0);
	float trailDistance = distance(worldPosition, start + delta * progress);
	return 1.0 - smoothstep(halfWidth, halfWidth + 0.45, trailDistance);
}

void main()
{
vec2 texcoords=vertex_tex;
float t=1./100.;
texcoords -= vec2(camoff.x,camoff.z)*t;

vec3 heightColor = texture(tex, texcoords).rgb;
float terrainVariation = 0.22 + heightColor.r * 0.78;
vec3 grassDetail = texture(tex2, texcoords * 50.0).rgb * terrainVariation;
float moonWeight = regionInfluence(vertex_pos.xz, moonRegionCenter,
                                   moonRegionRadii);
float redWeight = regionInfluence(vertex_pos.xz, redRegionCenter,
                                  redRegionRadii);
float specialTotal = moonWeight + redWeight;
if (specialTotal > 1.0)
{
	moonWeight /= specialTotal;
	redWeight /= specialTotal;
}
float meadowWeight = 1.0 - moonWeight - redWeight;
vec3 regionTone = meadowTerrainTint * meadowWeight +
                  moonTerrainTint * moonWeight +
                  redTerrainTint * redWeight;
float materialStrength = 0.08 + moonWeight * 0.46 + redWeight * 0.68;
vec3 baseColor = mix(grassDetail, regionTone * terrainVariation,
                     materialStrength);
float moonTrail = 0.0;
float redTrail = 0.0;
for (int trailIndex = 0; trailIndex < 9; ++trailIndex)
{
	float influence = trailInfluence(vertex_pos.xz,
	                                 trailSegments[trailIndex],
	                                 trailHalfWidths[trailIndex]);
	if (trailIndex < 4)
	{
		moonTrail = max(moonTrail, influence);
	}
	else
	{
		redTrail = max(redTrail, influence);
	}
}
float trailWeight = max(moonTrail, redTrail);
vec3 trailTone = moonTrail >= redTrail
	? vec3(0.28, 0.33, 0.30)
	: vec3(0.42, 0.29, 0.18);
vec3 trailMaterial = mix(grassDetail * 0.42,
	                      trailTone * (0.72 + terrainVariation * 0.28),
	                      0.78);
baseColor = mix(baseColor, trailMaterial, trailWeight * 0.82);
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
