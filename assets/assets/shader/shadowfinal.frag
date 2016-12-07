#version auto
precision mediump float;

in vec2 world_lightspace_frag;
in vec2 center_lightspace_frag;

out vec4 out_color;

uniform sampler2D distance_map_tex;
uniform sampler2D occlusions;

uniform int current_light_index;


vec3 sample_shadow_ray(vec2 tc, float r) {
	const float dist_per_arc = 0.02454; // tan(360^/numRays)*2.0

	float light_radius = 2.0; // TODO: get from uniform


	vec4 s = texture2D(distance_map_tex, tc);
	float occluder_dist = s.a;

	if(occluder_dist>=r) {
		occluder_dist = r/2.0;
	}

	float p = min(100.0*light_radius/2.0*r, max(0.0, light_radius*(r-occluder_dist)/r) / (dist_per_arc/r));
	
	vec3 color= vec3(0,0,0);
	for(float x=-8.0; x<=8.0; x+=1.0) {
		vec2 coord = vec2(tc.x + x/8.0 / 512.0 * p, tc.y);
		vec4 shadowmap = texture2D(distance_map_tex, coord);

		color.r += step(r, shadowmap.r);
		color.g += step(r, shadowmap.g);
		color.b += step(r, shadowmap.b);
	}

	color = clamp(color/(8.0*2.0+1.0), vec3(0.0), vec3(1.0));
	
	return color;
}


void main() {
	vec2 dir = center_lightspace_frag - world_lightspace_frag.xy;
	float dist = length(dir);
	dir /= dist;

	const float PI = 3.141;
	float theta = atan(dir.y, dir.x) + PI;

	vec2 tc = vec2(theta /(2.0*PI), (float(current_light_index)+0.5)/4.0);


	vec4 c = vec4(sample_shadow_ray(tc, dist), 1.0);

	vec4 occluder = texture2D(occlusions, world_lightspace_frag);
	c.rgb = min(c.rgb, step(occluder.rgb, vec3(0.2)));

	out_color = c;
}
