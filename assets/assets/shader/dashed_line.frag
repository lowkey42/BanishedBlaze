#version auto
precision mediump float;

in vec2 position_frag;

out vec4 out_color;

uniform vec2 p1;
uniform float dash_len;
uniform vec4 color;


void main() {
	if (cos(dash_len*abs(distance(p1, position_frag))) + 0.5 > 0.0) {
		discard;
	} else {
		out_color = color;
	}
}
