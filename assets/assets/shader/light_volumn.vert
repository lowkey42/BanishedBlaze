#version auto
precision mediump float;



in vec2 xy;
in vec2 uv;

out Vertex_out {
	vec2 world_uv;
	vec2 shadowmap_uv;
	vec2 shadowmap_luv;
	vec3 world_pos;
} output;


uniform mediump int current_light_index;

#include <_uniforms_globals.glsl>
#include <_uniforms_lighting.glsl>


void main() {
	vec3 position = light[current_light_index].pos.xyz + vec3(xy*light[current_light_index].area_of_effect*1.01, 0.0);
	position.z -= 0.5;

	vec4 view_pos = vp * vec4(position, 1.0);
	gl_Position = view_pos;


	vec4 pos_center = sse_vp * vec4(light[current_light_index].pos.xy, 0.0, 1);
	output.shadowmap_luv = pos_center.xy/pos_center.w;
	
	output.shadowmap_uv = view_pos.xy/view_pos.w * 0.5 + 0.5;
	output.world_uv = view_pos.xy / view_pos.w * 0.5 + 0.5;

	output.world_pos = position;
}
