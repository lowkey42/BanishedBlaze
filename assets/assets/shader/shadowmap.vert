#version 100
precision mediump float;

attribute vec2 xy;
attribute vec2 uv;

varying vec2 uv_frag;

void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	uv_frag = uv;
}
