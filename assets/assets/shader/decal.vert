#version auto
precision mediump float;

in vec2 position;
in vec2 uv;

out vec2 uvl;


#include <_uniforms_globals.glsl>


void main() {
	gl_Position = sse_vp * vec4(position.x, position.y, 0.0, 1.0);

	uvl = uv;
}
