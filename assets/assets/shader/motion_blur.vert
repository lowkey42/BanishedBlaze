#version 100
precision lowp float;

attribute vec2 xy;
attribute vec2 uv;

varying vec2 uv_center;
varying vec2 uv_1;
varying vec2 uv_2;
varying vec2 uv_3;

uniform bool horizontal;
uniform sampler2D texture;
uniform vec2 texture_size;
uniform vec2 dir;

void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	vec2 tex_offset = dir * (1.0 / texture_size);
	
	uv_center = uv;

	uv_1 = uv + 1.411764705882353 * tex_offset;
	uv_2 = uv + 3.2941176470588234 * tex_offset;
	uv_3 = uv + 5.176470588235294 * tex_offset;
}
