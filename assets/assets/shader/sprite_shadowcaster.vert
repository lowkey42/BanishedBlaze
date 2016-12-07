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
	vec3 pos;
	vec2 hue_change;
} output;

#include <_uniforms_globals.glsl>


void main() {
	vec4 pos_vp = sse_vp * vec4(position, 1);
	gl_Position = pos_vp;

	output.uv = uv;
	output.uv_clip = uv_clip;
	output.pos = position;
	output.hue_change = hue_change;
}

