#version 100
precision mediump float;

varying vec2 uv_frag;

uniform sampler2D occlusions;

uniform vec2 light_positions[8];


vec2 ndc2uv(vec2 p) {
	return clamp(p*0.5 + 0.5, vec2(0.0, 0.0), vec2(1.0, 1.0));
}

void main() {
	const float PI = 3.141;

	vec2 light_pos;
	if(uv_frag.y < 1.0/2.0) {
		light_pos = light_positions[0];
	} else {
		light_pos = light_positions[1];
	}

	float distance = 4.0;

	float theta = uv_frag.x * 2.0 * PI; //< uv_frag.x is our current percentage of the cirlce
	vec2 dir = vec2(cos(theta), sin(theta));

	for (float r=0.0; r<2.0; r+=1.0/(1024.0*2.0)) {
		vec2 coord = r*dir + light_pos;

		//sample the occlusion map
		float data = texture2D(occlusions, ndc2uv(coord)).r;

		if(data>=0.1) {
			distance = min(distance, r);
			break;
		}
	}

	distance = clamp(distance/4.0, 0.0, 1.0);

	gl_FragColor = vec4(distance, 0.0, 0.0, 1.0);
}
