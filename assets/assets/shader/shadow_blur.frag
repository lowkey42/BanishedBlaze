#version auto
precision lowp float;

in vec2 uv_center;
in vec2 uv_l1;
in vec2 uv_l2;
in vec2 uv_l3;
in vec2 uv_r1;
in vec2 uv_r2;
in vec2 uv_r3;

out vec4 out_color;

uniform sampler2D tex;


void main() {
	vec4 center = texture(tex, uv_center);

	vec4 result = center * 0.1964825501511404;
	result += texture(tex, uv_l1) * 0.2969069646728344;
	result += texture(tex, uv_l2) * 0.09447039785044732;
	result += texture(tex, uv_l3) * 0.010381362401148057;

	result += texture(tex, uv_r1) * 0.2969069646728344;
	result += texture(tex, uv_r2) * 0.09447039785044732;
	result += texture(tex, uv_r3) * 0.010381362401148057;

	out_color = result;
}
