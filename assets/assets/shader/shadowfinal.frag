#version auto
precision mediump float;

in Vertex_out {
	vec2 world_lightspace;
	vec2 center_lightspace;
	vec2 pos_worldspace;
	vec2 light_src_size;
} frag_in;

out vec4 out_color;

uniform sampler2D distance_map_tex;
uniform sampler2D occlusions;
uniform int current_light_index;

#include <_uniforms_globals.glsl>
#include <_uniforms_lighting.glsl>


vec3 sample_shadow_ray(vec2 tc, float r, float src_radius) {
	const float dist_per_arc = 0.02454; // tan(360^/numRays)*2.0

	float light_radius = light[current_light_index].src_radius;

	float world_dist = length(frag_in.pos_worldspace - light[current_light_index].flat_pos.xy);


	vec4 s = texture(distance_map_tex, tc);
	float occluder_dist = s.a;

	if(occluder_dist>=r) {
		occluder_dist = r/2.0;
	}

	float p = min(100.0*src_radius/2.0*r, max(0.0, src_radius*(r-occluder_dist)/r) / (dist_per_arc/r));

	// TODO: works better with *60.0, but why?
	p = max(0.0, src_radius*(r-occluder_dist)/r) / (dist_per_arc/world_dist*60.0);


	vec3 color= vec3(0,0,0);
	for(float x=-8.0; x<=8.0; x+=1.0) {
		vec2 coord = vec2(tc.x + x/8.0 / 512.0 * p, tc.y);
		vec4 shadowmap = texture(distance_map_tex, coord);

		color.r += step(r, shadowmap.r);
		color.g += step(r, shadowmap.g);
		color.b += step(r, shadowmap.b);
	}

	color = clamp(color/(8.0*2.0+1.0), vec3(0.0), vec3(1.0));
	
	return color;
}


void main() {
	vec2 dir = frag_in.center_lightspace - frag_in.world_lightspace.xy;
	float dist = length(dir);
	dir /= dist;

	const float PI = 3.141;
	float theta = atan(dir.y, dir.x) + PI;

	vec2 tc = vec2(theta /(2.0*PI), (float(current_light_index)+0.5)/4.0);

	float src_radius = frag_in.light_src_size.x;//abs(dot(frag_in.light_src_size.x, dir));

	vec4 c = vec4(sample_shadow_ray(tc, dist, src_radius), 1.0);

	vec4 occluder = texture(occlusions, frag_in.world_lightspace);
	c.rgb = min(c.rgb, step(occluder.rgb, vec3(0.2)));

	out_color = c;
}
