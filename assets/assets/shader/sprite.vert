#version auto
precision mediump float;

in vec3 position;
in vec2 decals_offset;
in vec2 uv;
in vec4 uv_clip;
in vec2 tangent;
in vec2 hue_change;
in float shadow_resistence;
in float decals_intensity;

out Vertex_out {
	vec2 uv;
	vec4 uv_clip;
	vec2 decals_uv;
	vec3 pos;
	vec2 hue_change;
	vec2 shadowmap_uv;
	float shadow_resistence;
	float decals_intensity;
	
	mat3 TBN;
} output;

#include <_uniforms_globals.glsl>


void main() {
	vec4 pos_vp = vp * vec4(position, 1);
	vec4 pos_lvp = vp * vec4(position.x, position.y, 0.0, 1);
	gl_Position = pos_vp;

	vec4 pos_vp0 = sse_vp * vec4(position.xy + decals_offset.xy, position.z/4.0, 1);
	output.decals_uv = pos_vp0.xy/pos_vp0.w/2.0+0.5;

	output.shadowmap_uv = pos_vp.xy/pos_vp.w/2.0+0.5;
	output.uv = uv;
	output.uv_clip = uv_clip;
	output.pos = position;
	output.hue_change = hue_change;
	output.shadow_resistence = shadow_resistence;
	output.decals_intensity = decals_intensity;

	// TODO: transform tangent and normal too!
	vec3 T = normalize(vec3(tangent,0.0));
	vec3 N = vec3(0.0,0.0,1.0);
	vec3 B = cross(N, T);
	output.TBN = mat3(T, B, N);
}

