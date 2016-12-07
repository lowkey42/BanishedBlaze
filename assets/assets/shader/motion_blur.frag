#version auto
precision lowp float;

in vec2 uv_center;
in vec2 uv_1;
in vec2 uv_2;
in vec2 uv_3;

out vec4 out_color;

uniform sampler2D tex;


void main() {
	vec3 result = texture(tex, uv_center).rgb * (1.0 - 0.2969069646728344 - 0.09447039785044732 - 0.010381362401148057);
	result += texture(tex, uv_1).rgb * 0.2969069646728344;
	result += texture(tex, uv_2).rgb * 0.09447039785044732;
	result += texture(tex, uv_3).rgb * 0.010381362401148057;

	out_color = vec4(result, 1.0);
}
