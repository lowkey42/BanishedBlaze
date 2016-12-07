#version auto
precision mediump float;

in vec2 position;
in vec2 uv;

out vec2 uvl;

uniform mat4 vp;
uniform mat4 model;
uniform float layer;


void main() {
	gl_Position = (vp*model) * vec4(position.x, position.y, layer, 1.0);

	uvl = uv;
}
