#version auto
precision mediump float;

in Vertex_out {
	vec2 world_uv;
	vec2 shadowmap_uv;
	vec2 shadowmap_luv;
	vec3 world_pos;
} input;

out vec4 out_color;


#include <_uniforms_lighting.glsl>
#include <_uniforms_globals.glsl>

uniform sampler2D shadowmap_tex;
uniform sampler2D depth_tex;
uniform mediump int current_light_index;


vec3 pixel_position(vec2 uv, float depth) {
	vec4 pos_view = vp_inv * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_view.xyz / pos_view.w;
}

float visibility(vec3 env_world_pos, float depth_offset) {
	return input.world_pos.z + depth_offset >= env_world_pos.z ? 1.0 : 0.0;
}

void main() {
	const float PI = 3.141;


	float tau = 0.00008; // density,  probability of collision
	float albedo = 0.25; // probability of scattering (not absorbing) after collision
	float phi = 10000.0;

	float radius = light[current_light_index].area_of_effect;

	vec2 uv = input.world_uv;
	float depth = texture2D(depth_tex, uv).r;
	vec3 env_world_pos = pixel_position(uv, depth);

	vec3 light_dir = light[current_light_index].pos.xyz - input.world_pos;
	float light_dist = length(light_dir);
	light_dir /= light_dist;

	light_dist = max(0.0, light_dist-light[current_light_index].src_radius);
	float light_dist2 = light_dist*light_dist;


	// calculate a: length of the ray that is in the light range (radius of the spherical cap)
	float h  = 1.0 - clamp(light_dist / radius, 0.0, 1.0);
	float a  = sqrt(max(0.0, 2.0*h - h*h));


	float s = a*2.0*radius;
	if(s<=0.0) {
		discard;
	}

	float L = 0.0;
	for(float i=1.0; i>=0.0; i-=0.2) {
		float l = s*i;
		float dl = s * 0.1;
		
		float r = s - a*radius;
		float d2 = light_dist2 + r*r; // distance to lightsource
		float d = sqrt(d2);

		float v = visibility(env_world_pos, r);

		float L_in = exp(-d * tau) * v * phi /4.0/PI/d2;
		float L_i  = L_in * tau * albedo; // TODO * P(?)
		L+= L_i * exp(-l * tau) * dl;
	}

	vec3 shadow = texture(shadowmap_tex, input.shadowmap_uv).rgb;
	
	out_color = vec4(light[current_light_index].color.rgb * L * shadow, 0.0);
}
