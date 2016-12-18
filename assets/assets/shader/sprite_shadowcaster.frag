#version auto
precision mediump float;


in Vertex_out {
	vec2 uv;
	vec4 uv_clip;
	vec3 pos;
	vec2 hue_change;
} frag_in;

out vec4 out_color;

uniform sampler2D albedo_tex;
uniform sampler2D height_tex;


vec3 rgb2hsv(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
	vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 hue_shift(vec3 in_color) {
	vec3 hsv = rgb2hsv(in_color);
	hsv.x = abs(hsv.x-frag_in.hue_change.x)<0.1 ? frag_in.hue_change.y+(hsv.x-frag_in.hue_change.x) : hsv.x;
	return hsv2rgb(hsv);
}

void main() {
	vec2 uv = mod(frag_in.uv, 1.0) * (frag_in.uv_clip.zw-frag_in.uv_clip.xy) + frag_in.uv_clip.xy;

	vec4 albedo = texture(albedo_tex, uv);
	vec3 color = hue_shift(albedo.rgb);
	float alpha = albedo.a;
	float height = texture(height_tex, uv).r;

	if(alpha < 0.01 || height < 0.7) {
		discard;
	}

	if(alpha<0.99) {
		out_color = vec4((vec3(1.0) - step(0.3, color)), 0.0);

	} else {
		out_color = vec4(1.0, 1.0, 1.0, 0.0);
	}
}

