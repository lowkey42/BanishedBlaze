#version 100
precision mediump float;

attribute vec3 position;
attribute vec2 decals_offset;
attribute vec2 uv;
attribute vec4 uv_clip;
attribute vec2 tangent;
attribute vec2 hue_change;
attribute float shadow_resistence;
attribute float decals_intensity;

varying vec2 uv_frag;
varying vec4 uv_clip_frag;
varying vec2 decals_uv_frag;
varying vec3 pos_frag;
varying vec2 hue_change_frag;
varying vec2 shadowmap_uv_frag;
varying float shadow_resistence_frag;
varying float decals_intensity_frag;

varying mat3 TBN;

uniform mat4 vp;
uniform mat4 vp_light;

void main() {
	vec4 pos_vp = vp * vec4(position, 1);
	vec4 pos_lvp = vp * vec4(position.x, position.y, 0.0, 1);
	gl_Position = pos_vp;

	vec4 pos_vp0 = vp_light * vec4(position.xy + decals_offset.xy, position.z/4.0, 1);
	decals_uv_frag = pos_vp0.xy/pos_vp0.w/2.0+0.5;

	shadowmap_uv_frag = pos_vp.xy/pos_vp.w/2.0+0.5;
	uv_frag = uv;
	uv_clip_frag = uv_clip;
	pos_frag = position;
	hue_change_frag = hue_change;
	shadow_resistence_frag = shadow_resistence;
	decals_intensity_frag = decals_intensity;

	vec3 T = normalize(vec3(tangent,0.0));
	vec3 N = vec3(0.0,0.0,1.0);
	vec3 B = cross(N, T);
	TBN = mat3(T, B, N);
}

