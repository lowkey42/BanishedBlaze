#version 100
precision mediump float;

varying vec2 shadowmap_uv_frag;
varying vec2 shadowmap_luv_frag;
varying vec2 world_uv_frag;
varying vec3 world_pos_frag;
varying vec3 light_pos_frag;
varying vec3 factors_frag;
varying vec3 color_frag;
varying float radius_frag;

uniform sampler2D shadowmap_tex;
uniform sampler2D depth_tex;
uniform mat4 vp_inv;
uniform mediump int current_light_index;


vec3 pixel_position(vec2 uv, float depth) {
	vec4 pos_view = vp_inv * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_view.xyz / pos_view.w;
}

float visibility(vec3 env_world_pos, float depth_offset) {
	return world_pos_frag.z + depth_offset >= env_world_pos.z ? 1.0 : 0.0;
}

void main() {
	const float PI = 3.141;


	float tau = 0.00006; // density,  probability of collision
	float albedo = 0.25; // probability of scattering (not absorbing) after collision
	float phi = 10000.0;

	vec2 uv = world_uv_frag;
	float depth = texture2D(depth_tex, uv).r;
	vec3 env_world_pos = pixel_position(uv, depth);

	vec3 light_dir = light_pos_frag - world_pos_frag;
	float light_dist = length(light_dir);
	float light_dist2 = light_dist*light_dist;
	light_dir /= light_dist;


	// calculate a: length of the ray that is in the light range (radius of the spherical cap)
	float h  = 1.0 - clamp(light_dist / radius_frag, 0.0, 1.0);
	float a  = sqrt(max(0.0, 2.0*h - h*h));


	float s = a*2.0*radius_frag;
	if(s<=0.0) {
		discard;
	}

	float L = 0.0;
	for(float i=1.0; i>=0.0; i-=0.2) {
		float l = s*i;
		float dl = s * 0.1;
		
		float r = s - a*radius_frag;
		float d2 = light_dist2 + r*r; // distance to lightsource
		float d = sqrt(d2);

		float v = visibility(env_world_pos, r);

		float L_in = exp(-d * tau) * v * phi /4.0/PI/d2;
		float L_i  = L_in * tau * albedo; // TODO * P(?)
		L+= L_i * exp(-l * tau) * dl;
	}

	vec3 shadow = texture2D(shadowmap_tex, shadowmap_uv_frag).rgb;
	
	gl_FragColor = vec4(color_frag * L * shadow, 0.0);
}
