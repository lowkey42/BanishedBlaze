#version 330 core
precision mediump float;

in vec2 shadowmap_uv_frag;
in vec2 shadowmap_luv_frag;
in vec2 world_uv_frag;
in vec3 world_pos_frag;
in vec3 light_pos_frag;
in vec3 factors_frag;
in vec3 color_frag;
in float radius_frag;

uniform sampler2D shadowmaps_tex;
uniform sampler2D depth_tex;
uniform mat4 vp_inv;
uniform mediump int current_light_index;


vec3 pixel_position(vec2 uv, float depth) {
	vec4 pos_view = vp_inv * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_view.xyz / pos_view.w;
}

float linstep(float low, float high, float v){
	return clamp((v-low)/(high-low), 0.0, 1.0);
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



vec3 sample_shadow_ray(vec2 tc, float r) {
	r/=2.0;
	
	vec3 color= vec3(0,0,0);
		
	for(float x=0.0; x<8.0; x+=1.0) {
			//vec4 shadowmap = texture2D(shadowmaps_tex, tc+vec2(x,0.0)*0.001);
			vec2 coord = vec2(tc.x + (random(tc+vec2(x))-0.5)*0.02, tc.y);
			vec4 shadowmap = texture2D(shadowmaps_tex, coord);
			
			color.r += smoothstep(r-0.001, r, shadowmap.r);
			color.g += smoothstep(r-0.001, r, shadowmap.g);
			color.b += smoothstep(r-0.001, r, shadowmap.b);
	}
	
	return color / (8.0);
	
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
