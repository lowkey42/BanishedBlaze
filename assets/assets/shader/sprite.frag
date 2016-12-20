#version auto
precision mediump float;

in Vertex_out {
	vec2 uv;
	vec4 uv_clip;
	vec2 decals_uv;
	vec3 pos;
	vec2 hue_change;
	vec2 shadowmap_uv;
	float shadow_resistence;
	float decals_intensity;
	
	mat3 TBN;
} frag_in;

out vec4 out_color;

#include <_uniforms_globals.glsl>
#include <lighting.frag>

uniform sampler2D albedo_tex;
uniform sampler2D specular_tex;
uniform sampler2D material_tex;
uniform sampler2D normal_tex;

uniform sampler2D decals_tex;
uniform samplerCube environment_tex;
uniform sampler2D last_frame_tex;

uniform float alpha_cutoff;


vec3 rgb2hsv(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
	vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 hue_shift(vec3 in_color) {
	vec3 hsv = rgb2hsv(in_color);
	hsv.x = abs(hsv.x-frag_in.hue_change.x)<0.1 ? frag_in.hue_change.y+(hsv.x-frag_in.hue_change.x) : hsv.x;
	return hsv2rgb(hsv);
}
vec4 process_albedo(vec4 c) {
	c.rgb = hue_shift(pow(c.rgb, vec3(2.2))) * c.a;
	return c;
}

void main() {
	vec2 uv = mod(frag_in.uv, 1.0) * (frag_in.uv_clip.zw-frag_in.uv_clip.xy) + frag_in.uv_clip.xy;

	vec4 albedo = process_albedo(texture(albedo_tex, uv));
	vec3 F0     = process_albedo(texture(specular_tex, uv)).rgb;

	if(albedo.a < alpha_cutoff) {
		discard;
	}
	albedo.rgb *= 5.0; // TODO

	vec3 normal = texture(normal_tex, uv).xyz;
	if(length(normal)<0.00001)
		normal = vec3(0,0,1);
	else {
		normal = normalize(normal*2.0 - 1.0);
	}
	vec3 N = normalize(frag_in.TBN * normal);

	vec3 material = texture(material_tex, uv).xyz;
	float emmision  = material.r;
	float roughness = 0.1;//TODO: mix(0.01, 0.99, material.b*material.b);
	float ambient_occlusion = material.g;


	float decals_fade = clamp(1.0+frag_in.pos.z/2.0, 0.25, 1.0) * frag_in.decals_intensity * albedo.a;
	vec4 decals = texture(decals_tex, frag_in.decals_uv);
	albedo.rgb = mix(albedo.rgb, decals.rgb * decals_fade * 0.5, decals.a * decals_fade);
	emmision  = mix(emmision,  0.3, max(0.0, decals.a * decals_fade - 0.5));
	roughness = mix(roughness, 0.2, decals.a * decals_fade);
	F0 = mix(F0, vec3(decals.rgb * decals_fade * 0.2), decals.a * decals_fade);


	vec3 V = normalize(frag_in.pos - eye.xyz);

	vec3 final_color = calc_light(frag_in.pos, albedo.rgb, F0, N, V, roughness)
	                 + albedo.rgb*emmision;
	out_color = vec4(final_color, 1.0) * albedo.a;
}

