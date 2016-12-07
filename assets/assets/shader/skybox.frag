#version auto
precision mediump float;

in vec3 position_frag;

out vec4 out_color;

uniform vec3 tint;
uniform float brightness;
uniform samplerCube tex;


void main() {
	out_color = vec4(pow(texture(tex, position_frag).rgb, vec3(2.2))*tint*brightness, 1.0);
}

