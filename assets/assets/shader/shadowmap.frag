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
	for (float dist=0.0; dist<1.0; dist+=1.0/(256.0)) {
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


// TODO: optimize
void main() {
	const float PI = 3.141;

	vec2 position = get_position();
	float theta = uv_frag.x * 2.0 * PI;
	vec2 dir = vec2(cos(theta), sin(theta));

	gl_FragColor = raycast(position, dir);
}
