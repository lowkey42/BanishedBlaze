#version auto
precision mediump float;

in vec2 uvl;

out vec4 out_color;

uniform sampler2D tex;
uniform vec4 color;
uniform vec4 clip;

void main() {
	vec4 c = texture(tex, uvl*clip.zw + clip.xy);
	c.rgb = pow(c.rgb, vec3(2.2)) * c.a;

	if(c.a>0.01) {
		out_color = c * color;
	} else
		discard;
}
