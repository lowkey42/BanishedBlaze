#version auto
precision mediump float;

in vec2 xy;
in vec2 uv;

out vec2 uv_frag;

void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	uv_frag = uv;
}
