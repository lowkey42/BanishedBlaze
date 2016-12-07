#version auto
precision mediump float;

in Vertex_out {
	float frames;
	float current_frame;
	float rotation;
	float alpha;
	float opacity;
	float hue_change_out;
} input;

out vec4 out_color;


uniform sampler2D tex;
uniform float hue_change_in;


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
	hsv.x = abs(hsv.x-hue_change_in)<0.01 ? input.hue_change_out : hsv.x;
	return hsv2rgb(hsv);
}
vec4 read_albedo(vec2 uv) {
	vec4 c = texture(tex, uv);
	c.rgb = hue_shift(pow(c.rgb, vec3(1.0)));
	return c;
}

vec2 rotate(vec2 p, float a) {
	vec2 r = mat2(cos(a), -sin(a), sin(a), cos(a)) * p;

	return vec2(r.x, -r.y);
}

void main() {
	vec2 uv = gl_PointCoord;
	uv.y = 1.0-uv.y;
	uv -= vec2(0.5, 0.5);
	uv = rotate(uv, input.rotation);
	uv += vec2(0.5, 0.5);
	uv.x = uv.x/input.frames + (input.current_frame)/input.frames;

	vec4 c = read_albedo(uv);

	out_color = vec4(c.rgb, input.opacity) * c.a * input.alpha;
}
