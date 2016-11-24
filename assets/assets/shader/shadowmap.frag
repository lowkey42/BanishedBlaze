#version 330 core
precision mediump float;

in vec2 uv_frag;
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

float randf(vec2 seed) {
	return fract(sin(dot(seed ,vec2(12.9898,78.233))) * 43758.5453);
}

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}



// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }



// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}



// Pseudo-random value in half-open range [0:1].
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
float random( vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec3  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec4  v ) { return floatConstruct(hash(floatBitsToUint(v))); }


vec3 rand(vec2 seed) {
	// TODO: better random
	return vec3(random(seed), random(seed+vec2(1.69075, 56.65857657)), random(seed+vec2(936.779, 246.65488)) );
}


vec3 raycast(vec2 position, vec2 dir) {
	vec3 distances = vec3(1,1,1);
	
	for (float dist=0.01; dist<1.0; dist+=1.0/(128.0)) {
		vec2 target = dist*dir*2.0 + position;
		vec2 oc_uv = ndc2uv(target);

		vec3 occluder = texture2D(occlusions, oc_uv).rgb;

		if(dot(occluder,occluder)>0.01*0.01) {
			vec3 eta = vec3(random(oc_uv));//rand(oc_uv)*1.0;

			if(occluder.r>=eta.r)
				distances.r = min(distances.r, dist);
			if(occluder.g>=eta.g)
				distances.g = min(distances.g, dist);
			if(occluder.b>=eta.b)
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
	float radius = 0.01;
	vec2 tangent = radius * vec2(-dir.y, dir.x);

	vec3 distances = raycast(position, dir);


	gl_FragColor = vec4(distances.rgb, 1.0);
}
