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

uniform sampler2D shadowmaps_tex;
uniform sampler2D depth_tex;
uniform mat4 vp_inv;
uniform int current_light_index;


vec3 pixel_position(vec2 uv, float depth) {
	vec4 pos_view = vp_inv * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_view.xyz / pos_view.w;
}

float linstep(float low, float high, float v){
	return clamp((v-low)/(high-low), 0.0, 1.0);
}
float smoothstep(float edge0, float edge1, float x) {
	x = clamp((x-edge0)/(edge1-edge0), 0.0, 1.0);
	return x*x*(3.0-2.0*x);
}

vec3 sample_shadow_ray(vec2 tc, float r) {
	vec4 shadowmap = texture2D(shadowmaps_tex, tc);

	//step(vec3(r/2.0), shadowmap.rgb);
	// TODO:


	return step(vec3(r/2.0), shadowmap.rgb);

	// TODO
	r/=2.0;
	float depth = min(shadowmap.r, min(shadowmap.g,shadowmap.b));
	float p = step(r, depth);
	float variance = max(shadowmap.a - depth*depth, -0.001);
	float d = depth-r;
	float p_max = linstep(0.1, 1.0, (variance / (variance + d*d))-0.4);
	return clamp(max(p, p_max), 0.0, 1.0) * vec3(1,1,1);
/*
	vec4 moments = shadowmap;

	vec3 d = moments.rgb - vec3(r/2.0);

	vec3 variance = vec3(moments.a) - (moments.rgb*moments.rgb);
	variance = max(variance, vec3(0.0000002));

	vec3 p_max = variance / (variance + d*d);
	p_max = p_max*2.0-1.0;
	p_max = pow(p_max,vec3(10.0));

	p_max = clamp(p_max, 0.0, 1.0);

	p_max.r = d.r>=0.0 ? 1.0 : p_max.r;
	p_max.g = d.g>=0.0 ? 1.0 : p_max.g;
	p_max.b = d.b>=0.0 ? 1.0 : p_max.b;
	return p_max;

*/
}

vec3 sample_shadow() {
	vec2 dir = shadowmap_luv_frag - shadowmap_uv_frag;
	float r = length(dir);
	dir /= r;

	const float PI = 3.141;
	float theta = atan(dir.y, dir.x) + PI;
	if(current_light_index>=4)
		return vec3(1.0,1.0,1.0);

	vec2 tc = vec2(theta /(2.0*PI), (float(current_light_index)+0.5)/4.0);

	return clamp(sample_shadow_ray(tc, r), vec3(0.0), vec3(1.0));
}

void main() {
	vec2 uv = world_uv_frag*vec2(0.5)+vec2(0.5);
	float depth = texture2D(depth_tex, uv).r;
	vec3 env_world_pos = pixel_position(uv, depth);

	vec3 light_dir = light_pos_frag - world_pos_frag;
	float light_dist = length(light_dir);
	light_dir /= light_dist;

	float h = 1.0 - clamp(light_dist / radius_frag, 0.0, 1.0);
	float a = sqrt(max(0.0, 2.0*h - h*h));
//	float intensity = 1.0/3.0 * a * (a+1.0) * (2.0*a+1.0);

	float light_dist2 = light_dist*light_dist;

	float intensity = 0.0;
	for(float i=-1.0; i<=1.0; i+=0.01) {
		float r = i*a;
		float a2 = r*radius_frag;

		if(world_pos_frag.z + a2 >= env_world_pos.z)
			intensity += 0.01 * max(0.0, 1.0 / ((light_dist2 + a2*a2)) -0.02);
	}

	//float attenuation = clamp(1.0 / (light_dist*light_dist*light_dist), 0.0, 1.0);

	vec3 color = color_frag * intensity*0.1;

	vec3 shadow = sample_shadow();

	gl_FragColor = vec4(color*shadow, 0.0);
	// *clamp(depth - gl_FragCoord.z, 0.0, 1.0)
	gl_FragColor = vec4(vec3(shadow)*0.5, 0.5);

	if(light_dist<1.0)
		gl_FragColor = vec4(1,1,1, 1.0);
}
