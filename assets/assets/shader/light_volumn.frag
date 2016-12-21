#version auto
precision mediump float;

in Vertex_out {
	vec2 world_uv;
	vec2 shadowmap_uv;
	vec2 shadowmap_luv;
	vec3 world_pos;
} frag_in;

out vec4 out_color;


#include <_uniforms_lighting.glsl>
#include <_uniforms_globals.glsl>

uniform sampler2D shadowmap_tex;
uniform sampler2D depth_tex;
uniform mediump int current_light_index;


float rand(vec2 c){
	return fract(sin(dot(c.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float noise(vec2 p, float freq ){
	const float PI = 3.141;
	
	float unit = 4.0/freq;
	vec2 ij = floor(p/unit);
	vec2 xy = mod(p,unit)/unit;
	xy = .5*(1.-cos(PI*xy));
	float a = rand((ij+vec2(0.,0.)));
	float b = rand((ij+vec2(1.,0.)));
	float c = rand((ij+vec2(0.,1.)));
	float d = rand((ij+vec2(1.,1.)));
	float x1 = mix(a, b, xy.x);
	float x2 = mix(c, d, xy.x);
	return mix(x1, x2, xy.y);
}

float pNoise(vec2 p, vec2 p2){
	float persistance = .5;
	float n = 0.;
	float normK = 0.;
	float f = 4.;
	float amp = 1.;
	
	n+=amp*noise(p, f);
	f*=2.;
	normK+=amp;
	amp*=persistance;
	
	n+=amp*noise(p2, f);
	f*=2.;
	normK+=amp;
	amp*=persistance;
	
	float nf = n/normK;
	return nf*nf*nf*nf;
}

vec3 pixel_position(vec2 uv, float depth) {
	vec4 pos_view = vp_inv * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_view.xyz / pos_view.w;
}

float visibility(vec3 env_world_pos, float depth_offset) {
	return frag_in.world_pos.z + depth_offset >= env_world_pos.z ? 1.0 : 0.0;
}

void main() {
	const float PI = 3.141;
	const float PI_rcp = 0.31830986;

	const float tau = 0.00008; // density,  probability of collision
	const float albedo = 0.25; // probability of scattering (not absorbing) after collision
	const float phi = 5000.0;
	
	vec3 shadow = texture(shadowmap_tex, frag_in.shadowmap_uv).rgb;
	
	if(dot(shadow,shadow)<=0.0)
		discard;
	
	float radius = light[current_light_index].area_of_effect;

	vec2 uv = frag_in.world_uv;
	float depth = texture2D(depth_tex, uv).r;
	vec3 env_world_pos = pixel_position(uv, depth);


	float s = radius;
	float dl = s * 2.0 * 0.1;
	if(s<=0.0) {
		discard;
	}

	vec3 light_dir = vec3(cos(-light[current_light_index].direction),
	                      sin(-light[current_light_index].direction),
	                      0.0);
	float angle_cos = cos(light[current_light_index].angle);
	
	float L = 0.0;
	for(float i=1.0; i>=0.0; i-=0.1) {
		float r = s*(i*2.0-1.0);
		
		vec3 diff = light[current_light_index].pos.xyz - (frag_in.world_pos + vec3(0,0,r));
		float d = length(diff);

		float v = visibility(env_world_pos, r);
		
		float denom = d/light[current_light_index].src_radius + 1.0;
		float attenuation = clamp(1.0 / (denom*denom) -0.0001, 0.0, 1.0) / (1.0-0.0001);
		attenuation = pow(attenuation,0.5);
		
		if(light[current_light_index].angle<1.999*PI) {
			float spot_factor = dot(diff/d, light_dir);
			attenuation *= 1.0 - (1.0 - spot_factor) / (1.0 - angle_cos);
		}
		
		float dRcp = 1.0/d;
		float L_i = tau * v * phi*albedo*PI_rcp* dRcp*dRcp * exp(-d*tau)*exp(-1*tau) * dl * attenuation;
		L+=L_i;
	}
	
	vec2 fog_dir_a = vec2(0.4, -0.08);
	vec2 fog_dir_b = vec2(0.3, 0.08);
	float fog = mix(1.0,pNoise(frag_in.world_pos.xy+fog_dir_a*time,
	                           frag_in.world_pos.xy+fog_dir_b*time), 0.5);
	
	out_color = vec4(light[current_light_index].color.rgb * L * shadow * fog, 0.0);
}
