#version 330 core
precision mediump float;

struct Point_light {
	vec3 pos;
	float dir;
	float angle;
	float radius;
	vec3 color;
	vec3 factors;
};


in vec2 xy;
in vec2 uv;

out vec2 world_uv_frag;
out vec2 shadowmap_uv_frag;
out vec2 shadowmap_luv_frag;
out vec3 world_pos_frag;
out vec3 light_pos_frag;
out vec3 factors_frag;
out vec3 color_frag;
out float radius_frag;

uniform Point_light light[8];
uniform mediump int current_light_index;

uniform mat4 vp;
uniform mat4 vp_light;



void main() {
	vec3 position = light[current_light_index].pos + vec3(xy*light[current_light_index].radius*1.2, 0.0);
	position.z -= 0.5;

	vec4 view_pos = vp * vec4(position, 1.0);
	gl_Position = view_pos;

	vec4 pos_light = vp_light * vec4(position, 1);

	shadowmap_uv_frag = pos_light.xy/pos_light.w;

	vec4 pos_center = vp_light * vec4(light[current_light_index].pos.xy, 0.0, 1);
	shadowmap_luv_frag = pos_center.xy/pos_center.w;

	world_uv_frag = view_pos.xy / view_pos.w;

	world_pos_frag = position;
	light_pos_frag = light[current_light_index].pos;
	factors_frag = light[current_light_index].factors;
	color_frag = light[current_light_index].color;
	radius_frag = light[current_light_index].radius;
}
