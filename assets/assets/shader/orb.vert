#version auto
precision mediump float;

in vec2 position;
in vec2 uv;

out vec2 uv_frag;
out vec2 uv_mask_frag;

uniform mat4 vp;
uniform vec2 pos;
uniform float scale;
uniform float time;


vec2 rotate(vec2 p, float a) {
	vec2 r = mat2(cos(a), -sin(a), sin(a), cos(a)) * p;

	return vec2(r.x, -r.y);
}

void main() {
	gl_Position = vp * vec4(position.x*scale+pos.x, position.y*scale+pos.y, 0.0, 1.0);

	vec2 ruv = rotate(uv*vec2(2.0)-vec2(1.0), time)/vec2(2.0)+vec2(0.5);
	vec2 nruv = rotate(uv*vec2(2.0)-vec2(1.0), -time)/vec2(2.0)+vec2(0.5);
	uv_frag = vec2(ruv.x/2.0, ruv.y);
	uv_mask_frag = vec2(nruv.x/2.0 + 0.5, nruv.y);
}
