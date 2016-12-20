#version auto
precision mediump float;

in vec2 xy;
in vec2 uv;

out Vertex_out {
	vec2 world_lightspace;
	vec2 center_lightspace;
	vec2 pos_worldspace;
	vec2 light_src_size;
} vert_out;

#include <_uniforms_globals.glsl>
#include <_uniforms_lighting.glsl>

uniform vec2 center_lightspace;
uniform int current_light_index;


void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	vec4 zero = vp * vec4(0,0,0,1);

	vec4 world_pos = vp_inv * vec4(xy.x*2.0, xy.y*2.0, zero.z/zero.w, 1.0);
	world_pos/=world_pos.w;
	world_pos.z=0.0;

	vert_out.pos_worldspace = world_pos.xy;

	vec4 light_pos = sse_vp * world_pos;
	vert_out.world_lightspace  = light_pos.xy / light_pos.w * 0.5+0.5;
	vert_out.center_lightspace = center_lightspace * 0.5+0.5;

	vec4 size = sse_vp * vec4(light[current_light_index].src_radius, light[current_light_index].src_radius, 0,0);
	vert_out.light_src_size = size.xy;
}
