#version auto
precision mediump float;

in vec3 position;
out vec3 position_frag;

#include <_uniforms_globals.glsl>


void main() {
	float a = eye.x * 0.001;
	vec3 p = vec3(position.x*cos(a)+position.z*sin(a),
	                     position.y,
	                     -position.x*sin(a)+position.z*cos(a));

	vec4 pos = vp * vec4(p*10000.0 + eye.xyz, 1.0);
	gl_Position = pos.xyww;

	position_frag = position;
}
