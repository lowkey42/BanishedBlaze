#version auto
precision mediump float;

in vec2 uv_frag;

out vec4 out_color;

uniform sampler2D tex;


void main() {
	vec3 color = texture(tex, uv_frag).rgb;
	float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722)) * 1.0;

	if(brightness>0.01)
		out_color = vec4(color * clamp(brightness,0.0,1.0), 1.0);
	else
		out_color = vec4(0.0,0.0,0.0, 1.0);
}
