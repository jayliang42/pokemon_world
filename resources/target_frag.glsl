#version 410 core
out vec4 color;

in vec2 vertex_tex;

uniform float time;
uniform vec3 ringColor;
uniform float opacity;
uniform float fillAmount;

void main()
{
	vec2 centered = vertex_tex - vec2(0.5);
	float radius = length(centered) * 2.0;
	float outerEdge = 1.0 - smoothstep(0.88, 1.0, radius);
	float innerEdge = smoothstep(0.62, 0.73, radius);
	float ring = outerEdge * innerEdge;
	float angle = atan(centered.y, centered.x);
	float ticks = smoothstep(0.72, 0.92, radius) *
	              smoothstep(0.90, 0.98, abs(cos(angle * 4.0)));
	float pulse = 0.78 + 0.22 * sin(time * 4.5);
	float ringAlpha = max(ring * pulse, ticks) * opacity;
	float softDisc = 1.0 - smoothstep(0.12, 1.0, radius);
	float alpha = mix(ringAlpha, softDisc * opacity, fillAmount);
	if (alpha < 0.01)
	{
		discard;
	}
	vec3 finalColor = mix(ringColor * (0.78 + pulse * 0.35),
	                      ringColor, fillAmount);
	color = vec4(finalColor, alpha);
}
