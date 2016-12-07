#version auto
precision mediump float;

in vec3 position;
in vec3 direction;
in float rotation;
in float frames;
in float current_frame;
in float size;
in float alpha;
in float opacity;
in float hue_change_out;

out Vertex_out {
	float frames;
	float current_frame;
	float rotation;
	float alpha;
	float opacity;
	float hue_change_out;
} output;

#include <_uniforms_globals.glsl>


void main() {
	vec2 dir_view = (view * vec4(direction, 0.0)).xy;
	float dir_view_len = length(dir_view);
	float rot_dyn = 0.0;
	if(dir_view_len > 0.01) {
		dir_view /= dir_view_len;

		if(abs(dir_view.x)>0.0)
			rot_dyn = atan(dir_view.y, dir_view.x);
		else
			rot_dyn = asin(dir_view.y);
	}

	vec4 clip_space_pos = vp * vec4(position, 1.0);
	clip_space_pos/=clip_space_pos.w;

	gl_Position = clip_space_pos;
	gl_PointSize = (1.0-clip_space_pos.z) * size * 200.0;

	output.frames = frames;
	output.current_frame = floor(current_frame);
	output.rotation = rotation+rot_dyn;
	output.alpha = alpha;
	output.opacity = opacity;
	output.hue_change_out = hue_change_out;
}
