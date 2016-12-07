#version auto
precision mediump float;

in vec2 uvl;

out vec4 out_color;

uniform sampler2D albedo_tex;


void main() {
	vec4 c = texture(albedo_tex, uvl);

	out_color = vec4(pow(c.rgb, vec3(2.2)) * c.a, 1.0);
}
