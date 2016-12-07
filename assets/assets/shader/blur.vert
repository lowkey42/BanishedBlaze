#version auto
precision lowp float;

in vec2 xy;
in vec2 uv;

out vec2 uv_center;
out vec2 uv_l1;
out vec2 uv_l2;
out vec2 uv_l3;
out vec2 uv_r1;
out vec2 uv_r2;
out vec2 uv_r3;

uniform bool horizontal;
uniform vec2 texture_size;


void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	vec2 tex_offset = 1.0 / texture_size;
	if(horizontal)
		tex_offset.y=0.0;
	else
		tex_offset.x=0.0;
	
	uv_center = uv;

	uv_l1 = uv + 1.411764705882353 * tex_offset;
	uv_l2 = uv + 3.2941176470588234 * tex_offset;
	uv_l3 = uv + 5.176470588235294 * tex_offset;

	uv_r1 = uv - 1.411764705882353 * tex_offset;
	uv_r2 = uv - 3.2941176470588234 * tex_offset;
	uv_r3 = uv - 5.176470588235294 * tex_offset;
}
