#version auto
precision mediump float;

in vec2 uvl;

out vec4 out_color;

uniform sampler2D tex;
uniform vec4 color;
uniform vec4 clip;

void main() {
	vec4 c = texture(tex, uvl);

	if(c.a>0.1) {
		out_color = c;
	} else
		out_color = vec4(1.0, 0.0, 1.0, 0.5);
}
