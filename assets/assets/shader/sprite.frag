#version 100
precision mediump float;

struct Dir_light {
	vec3 color;
	vec3 dir;
};
struct Point_light {
	vec3 pos;
	float dir;
	float angle;
	vec3 color;
	vec3 factors;
};

varying vec2 uv_frag;
varying vec4 uv_clip_frag;
varying vec2 decals_uv_frag;
varying vec3 pos_frag;
varying vec2 shadowmap_uv_frag;
varying vec2 hue_change_frag;
varying float shadow_resistence_frag;
varying float decals_intensity_frag;
varying mat3 TBN;

uniform sampler2D albedo_tex;
uniform sampler2D normal_tex;
uniform sampler2D material_tex;
uniform sampler2D height_tex;

uniform sampler2D shadowmaps_tex;

uniform sampler2D decals_tex;
uniform samplerCube environment_tex;
uniform sampler2D last_frame_tex;

uniform float light_ambient;
uniform Dir_light light_sun;
uniform Point_light light[8];

uniform vec3 eye;
uniform float alpha_cutoff;
uniform bool fast_lighting;

#include <lighting.frag>

#skip begin
float my_smoothstep(float edge0, float edge1, float x);
float calc_shadow(int light_num);
vec3 calc_point_light(Point_light light, vec3 normal, vec3 albedo, vec3 view_dir,
                      float roughness, float metalness, float reflectance);
vec3 calc_dir_light(Dir_light light, vec3 normal, vec3 albedo, vec3 view_dir,
                    float roughness, float metalness, float reflectance);
#skip end


vec3 rgb2hsv(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
	vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 hue_shift(vec3 in_color) {
	vec3 hsv = rgb2hsv(in_color);
	hsv.x = abs(hsv.x-hue_change_frag.x)<0.1 ? hue_change_frag.y+(hsv.x-hue_change_frag.x) : hsv.x;
	return hsv2rgb(hsv);
}
vec4 read_albedo(vec2 uv) {
	vec4 c = texture2D(albedo_tex, uv);
	c.rgb = hue_shift(pow(c.rgb, vec3(2.2))) * c.a;
	return c;
}

void main() {
	vec2 uv = mod(uv_frag, 1.0) * (uv_clip_frag.zw-uv_clip_frag.xy) + uv_clip_frag.xy;

	vec4 albedo = read_albedo(uv);
	vec3 normal = texture2D(normal_tex, uv).xyz;
	if(length(normal)<0.00001)
		normal = vec3(0,0,1);
	else {
		normal = normalize(normal*2.0 - 1.0);
	}
	normal = TBN * normal;

	vec3 material = texture2D(material_tex, uv).xyz;
	float emmision = material.r;
	float metalness = material.g;
	float smoothness = clamp(1.0-material.b, 0.01, 0.99);

	if(albedo.a < alpha_cutoff) {
		discard;
	}

	float decals_fade = clamp(1.0+pos_frag.z/2.0, 0.25, 1.0) * decals_intensity_frag * albedo.a;
	vec4 decals = texture2D(decals_tex, decals_uv_frag);
	albedo.rgb = mix(albedo.rgb, decals.rgb * decals_fade, decals.a * decals_fade);
	emmision = mix(emmision, 0.3, max(0.0, decals.a * decals_fade - 0.5));
	smoothness = mix(smoothness, 0.4, decals.a * decals_fade);
	metalness = mix(metalness, 0.6, decals.a * decals_fade);


	float roughness = 1.0 - smoothness*smoothness;
	float reflectance = clamp((0.9-roughness)*1.1 + metalness*0.1, 0.0, 1.0);

	vec3 view_dir = normalize(pos_frag-vec3(eye.xy, eye.z*10.0));

	vec3 ambient = fast_lighting ? vec3(light_ambient*0.5) : (pow(textureCube(environment_tex, normal, 10.0).rgb, vec3(2.2)) * light_ambient);
	vec3 color = vec3(0.0);


	if(fast_lighting) {
		for(int i=0; i<4; i++) {
			color += calc_point_light(light[i], normal, albedo.rgb, view_dir, roughness, metalness, reflectance);
		}

	} else {
		for(int i=0; i<2; i++) {
			color += calc_point_light(light[i], normal, albedo.rgb, view_dir, roughness, metalness, reflectance) ;//* calc_shadow(i);
		}
		for(int i=2; i<8; i++) {
			color += calc_point_light(light[i], normal, albedo.rgb, view_dir, roughness, metalness, reflectance);
		}
	}

	// in low-light scene, discard colors but keep down-scaled luminance
	color = mix(color, vec3(my_smoothstep(0.1, 0.2, pow(luminance(albedo.rgb), 3.3)*6000.0))*0.007, my_smoothstep(0.015, 0.005, length(color)));

	color += albedo.rgb * ambient;
	color += calc_dir_light(light_sun, normal, albedo.rgb, view_dir, roughness, metalness, reflectance);

	if(!fast_lighting) {
		vec3 refl = pow(textureCube(environment_tex, reflect(view_dir,normal), 10.0*roughness).rgb, vec3(2.2));
		refl *= mix(vec3(1,1,1), albedo.rgb, metalness);
		color += refl*reflectance*0.2;
	}

	color = mix(color, albedo.rgb*3.0, emmision);

	gl_FragColor = vec4(color, albedo.a);
}

