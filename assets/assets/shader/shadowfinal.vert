#version auto
precision mediump float;

in vec2 xy;
in vec2 uv;

out Vertex_out {
	vec2 world_lightspace;
	vec2 center_lightspace;
} output;

#include <_uniforms_globals.glsl>

uniform vec2 center_lightspace;


void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	vec4 zero = vp * vec4(0,0,0,1);

	vec4 world_pos = vp_inv * vec4(xy.x*2.0, xy.y*2.0, zero.z/zero.w, 1.0);
	world_pos/=world_pos.w;
	world_pos.z=0.0;

	vec4 light_pos = sse_vp * world_pos;
	output.world_lightspace  = light_pos.xy / light_pos.w * 0.5+0.5;
	output.center_lightspace = center_lightspace * 0.5+0.5;
}
