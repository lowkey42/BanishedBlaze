#version 330 core
precision mediump float;

in vec2 xy;
in vec2 uv;

out vec2 world_lightspace_frag;
out vec2 center_lightspace_frag;

uniform mat4 vp;
uniform mat4 vp_inv;
uniform mat4 vp_light;
uniform vec2 center_lightspace;

void main() {
	gl_Position = vec4(xy.x*2.0, xy.y*2.0, 0.0, 1.0);

	vec4 zero = vp * vec4(0,0,0,1);

	vec4 world_pos = vp_inv * vec4(xy.x*2.0, xy.y*2.0, zero.z/zero.w, 1.0);
	world_pos/=world_pos.w;
	world_pos.z=0.0;

	vec4 light_pos = vp_light * world_pos;
	world_lightspace_frag = light_pos.xy / light_pos.w *0.5+0.5;
	center_lightspace_frag = center_lightspace*0.5+0.5;
}
