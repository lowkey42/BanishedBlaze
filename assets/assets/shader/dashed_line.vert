#version auto
precision mediump float;

in float index;

out vec2 position_frag;

#include <_uniforms_globals.glsl>

uniform vec2 p1;
uniform vec2 p2;


void main() {
	vec2 pos = index<=0.0 ? p1 : p2;

	gl_Position = vp * vec4(pos, 0.0, 1.0);

	position_frag = pos;
}
