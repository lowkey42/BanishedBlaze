#version 330 core
precision mediump float;

in vec2 xy_frag;
in vec2 world_lightspace_frag;

uniform sampler2D distance_map_tex;
uniform sampler2D occlusions;

uniform vec2 center_lightspace;
uniform int current_light_index;

uniform mat4 vp_inv;
uniform mat4 vp_light;


vec3 sample_shadow_ray(vec2 tc, float r) {
	const float dist_per_arc = 0.02454; // tan(360^/numRays)*2.0

	float light_radius = 2.0; // TODO: get from uniform

	r/=2.0;

	vec3 color= vec3(0,0,0);

	vec4 s = texture2D(distance_map_tex, tc);
	float occluder_dist = s.a;

	if(occluder_dist>=r) {
		occluder_dist = r/2.0;
	}

	float p = min(100.0*light_radius/2.0*r, max(0.0, light_radius*(r-occluder_dist)/r) / (dist_per_arc/r));

	for(float x=-16.0; x<=16.0; x+=1.0) {
		vec2 coord = vec2(tc.x + x/16.0 / 512.0 * p, tc.y);
		vec4 shadowmap = texture2D(distance_map_tex, coord);

		color.r += step(r, shadowmap.r);
		color.g += step(r, shadowmap.g);
		color.b += step(r, shadowmap.b);
	}

	return color / (16.0*2.0+1.0);
}


void main() {
	vec4 world_pos = vp_inv * vec4(xy_frag, 1.0, 1.0);
	world_pos/=world_pos.w;

	world_pos.z=0.0;

	vec4 light_pos = vp_light * world_pos;
	light_pos /= light_pos.w;

	vec2 dir = center_lightspace - world_lightspace_frag.xy;
	float dist = length(dir);
	dir /= dist;

	const float PI = 3.141;
	float theta = atan(dir.y, dir.x) + PI;

	vec2 tc = vec2(theta /(2.0*PI), (float(current_light_index)+0.5)/4.0);


	vec4 shadow = texture2D(distance_map_tex, tc);
	float p = min(1000.0*dist, max(0.0, 0.25*(dist-shadow.a)/dist) / (0.01227/dist));
	gl_FragColor = vec4(step(dist/2.0, shadow.rgb), (dist-shadow.a)/dist);

	gl_FragColor.rgb = sample_shadow_ray(tc, dist);

	vec4 occluder = texture2D(occlusions, world_lightspace_frag*0.5+0.5);
	if(occluder.r>=0.2) gl_FragColor.r=0.0;
	if(occluder.g>=0.2) gl_FragColor.g=0.0;
	if(occluder.b>=0.2) gl_FragColor.b=0.0;

	gl_FragColor.a = 1.0;
}
