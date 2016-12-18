#version auto
precision mediump float;

in vec2 uv_frag;

out vec4 out_color;

#include <_uniforms_lighting.glsl>

uniform sampler2D occlusions;


vec2 ndc2uv(vec2 p) {
	return clamp(p*0.5 + 0.5, vec2(0.0, 0.0), vec2(1.0, 1.0));
}

vec2 get_position() {
	if(uv_frag.y<1.0/4.0)
		return light[0].flat_pos.xy;
	else if(uv_frag.y<2.0/4.0)
		return light[1].flat_pos.xy;
	else if(uv_frag.y<3.0/4.0)
		return light[2].flat_pos.xy;
	else
		return light[3].flat_pos.xy;

	return vec2(1000,1000);
}

vec4 raycast(vec2 position, vec2 dir) {
	vec4 distances = vec4(1,1,1,1)*100.0;
	for (float dist=0.0; dist<1.0; dist+=1.0/(256.0)) {
		vec2 target = dist*dir*2.0 + position;

		vec3 occluder = texture(occlusions, ndc2uv(target)).rgb;

		if(dot(occluder,occluder)>0.5*0.5) {
			distances.a = min(distances.a, dist);

			if(occluder.r>=0.1)
				distances.r = min(distances.r, dist);
			if(occluder.g>=0.1)
				distances.g = min(distances.g, dist);
			if(occluder.b>=0.1)
				distances.b = min(distances.b, dist);
		}
	}

	return distances;
}


void main() {
	const float PI = 3.141;

	vec2 position = get_position();
	float theta = uv_frag.x * 2.0 * PI;
	
	float theta_local = theta + light[int(uv_frag.y*4.0)].direction;
	theta_local = ((theta_local/(2.0*PI)) - floor(theta_local/(2.0*PI))) * 2.0*PI - PI;
	if(abs(theta_local) > light[int(uv_frag.y*3.0)].angle){out_color=vec4(0,0,0,0); return;}
	
	vec2 dir = vec2(cos(theta), sin(theta));

	out_color = raycast(position, dir);
}
