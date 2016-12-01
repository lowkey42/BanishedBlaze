#version 100
precision mediump float;

varying vec2 uv_frag;
uniform sampler2D occlusions;

uniform vec2 light_positions[4];
uniform float light_quality;


vec2 ndc2uv(vec2 p) {
	return clamp(p*0.5 + 0.5, vec2(0.0, 0.0), vec2(1.0, 1.0));
}

vec2 get_position() {
	if(uv_frag.y<1.0/4.0)
		return light_positions[0];
	else if(uv_frag.y<2.0/4.0)
		return light_positions[1];
	else if(uv_frag.y<3.0/4.0)
		return light_positions[2];
	else
		return light_positions[3];

	return vec2(1000,1000);
}

vec4 raycast(vec2 position, vec2 dir) {
	vec4 distances = vec4(1,1,1,1)*100.0;
	for (float dist=0.01; dist<1.0; dist+=1.0/(256.0)) {
		vec2 target = dist*dir*2.0 + position;

		vec3 occluder = texture2D(occlusions, ndc2uv(target)).rgb;

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

float pack_vec2(vec2 input) {
	input.x = floor(input.x * (4096.0-1.0));
	input.y = floor(input.y * (4096.0-1.0));

	return (input.x*4096.0) + input.y;
}

// TODO: optimize
void main() {
	const float PI = 3.141;

	vec2 position = get_position();
	float theta = uv_frag.x * 2.0 * PI;
	vec2 dir = vec2(cos(theta), sin(theta));
	float radius = 0.01;
	float tangent = radius * vec2(-dir.y, dir.x);

	vec4 distances = raycast(position, dir);
//	float distance_left = raycast(position + tangent, dir).a;
//	float distance_right = raycast(position - tangent, dir).a;


	float depth = distances.a;
	float dx = dFdx(depth);
	float moment = depth*depth + 0.25*(dx*dx);

	gl_FragColor = vec4(distances.rgba);//pack_vec2(vec2(distance_left, distance_right)));

	/*
	vec2 position = get_position();
	float theta = uv_frag.x * 2.0 * PI;
	vec2 dir = vec2(cos(theta), sin(theta));

	vec3 distances = vec3(1,1,1);
	for (float dist=0.0; dist<1.0; dist+=1.0/(1024.0)) {
		vec2 target = dist*dir*2.0 + position;

		vec3 occluder = texture2D(occlusions, ndc2uv(target)).rgb;

		if(occluder.r>=0.999)
			distances.r = min(distances.r, dist);
		if(occluder.g>=0.999)
			distances.g = min(distances.g, dist);
		if(occluder.b>=0.999)
			distances.b = min(distances.b, dist);
	}

	float depth = min(distances.r, min(distances.g,distances.b));
	float dx = dFdx(depth);
	float moment = depth*depth + 0.25*(dx*dx);

	if(depth<=0.01)
		gl_FragColor = vec4(0,0,0,0);
	else
		gl_FragColor = vec4(distances, moment);
	*/
}
