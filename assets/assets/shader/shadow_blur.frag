#version 100
precision lowp float;


varying vec2 uv_center;
varying vec2 uv_l1;
varying vec2 uv_l2;
varying vec2 uv_l3;
varying vec2 uv_r1;
varying vec2 uv_r2;
varying vec2 uv_r3;

uniform sampler2D texture;


void main() {
	vec4 center = texture2D(texture, uv_center);

	vec4 result = center * 0.1964825501511404;
	result += texture2D(texture, uv_l1) * 0.2969069646728344;
	result += texture2D(texture, uv_l2) * 0.09447039785044732;
	result += texture2D(texture, uv_l3) * 0.010381362401148057;

	result += texture2D(texture, uv_r1) * 0.2969069646728344;
	result += texture2D(texture, uv_r2) * 0.09447039785044732;
	result += texture2D(texture, uv_r3) * 0.010381362401148057;

	gl_FragColor = result;
}
