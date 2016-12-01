
#skip begin
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

uniform sampler2D shadowmap_0_tex;
uniform sampler2D shadowmap_1_tex;
uniform sampler2D shadowmap_2_tex;
uniform sampler2D shadowmap_3_tex;

uniform sampler2D decals_tex;
uniform samplerCube environment_tex;
uniform sampler2D last_frame_tex;

uniform vec3 eye;
uniform float alpha_cutoff;
uniform bool fast_lighting;
#skip end


float G1V ( float dotNV, float k ) {
	return 1.0 / (dotNV*(1.0 - k) + k);
}
vec3 calc_light(vec3 light_dir, vec3 light_color, vec3 normal, vec3 albedo, vec3 view_dir, float roughness, float metalness, float reflectance) {

	// TODO: cleanup this mess and replace experiments with real formulas
	vec3 N = normal;
	vec3 V = -view_dir;

	float alpha = roughness*roughness;
	vec3 L = normalize(light_dir);
	vec3 H = normalize (V + L);

	float dotNL = clamp (dot (N, L), 0.0, 1.0);
	float dotNV = clamp (dot (N, V), 0.0, 1.0);
	float dotNH = clamp (dot (N, H), 0.0, 1.0);
	float dotLH = clamp (dot (L, H), 0.0, 1.0);

	float D, vis;
	vec3 F;

	// NDF : GGX
	float alphaSqr = alpha*alpha;
	float pi = 3.1415926535;
	float denom = dotNH * dotNH *(alphaSqr - 1.0) + 1.0;
	D = alphaSqr / (pi * denom * denom);

	float spec_mod_factor = 2.0 + metalness*40.0; //< not physicaly accurate, but makes metals more shiny
	D*=mix(spec_mod_factor, 0.0, roughness); // TODO(this is a workaround): specular is to powerfull for realy rough surfaces
	D = clamp(D, 0.0, 50.0);

	// Fresnel (Schlick)
	vec3 F0 = mix(vec3(0.16*reflectance*reflectance), albedo.rgb, 0.0);
	float dotLH5 = pow (1.0 - dotLH, 5.0);
	F = F0 + (1.0 - F0)*(dotLH5);

	// Visibility term (G) : Smith with Schlick's approximation
	float k = alpha / 2.0;
	vis = G1V (dotNL, k) * G1V (dotNV, k);

	vec3 specular = D * F * vis;

	float invPi = 0.31830988618;
	vec3 diffuse = (albedo * invPi);

	diffuse*=(1.0 - metalness);

	light_color = mix(light_color, light_color*albedo, metalness);

	return (diffuse + specular) * light_color * dotNL;
}

float my_smoothstep(float edge0, float edge1, float x) {
	x = clamp((x-edge0)/(edge1-edge0), 0.0, 1.0);
	return x*x*(3.0-2.0*x);
}

vec3 calc_shadow(int light_num) {
	vec3 shadow = vec3(1,1,1);

	if(light_num==0)
		shadow = texture2D(shadowmap_0_tex, shadowmap_uv_frag).rgb;
	else if(light_num==1)
		shadow = texture2D(shadowmap_1_tex, shadowmap_uv_frag).rgb;
	else if(light_num==2)
		shadow = texture2D(shadowmap_2_tex, shadowmap_uv_frag).rgb;
	else if(light_num==3)
		shadow = texture2D(shadowmap_3_tex, shadowmap_uv_frag).rgb;

	return shadow; // TODO
	return mix(shadow, vec3(1.0), shadow_resistence_frag*0.9);
}

vec3 calc_point_light(Point_light light, vec3 normal, vec3 albedo, vec3 view_dir, float roughness, float metalness, float reflectance) {
	vec3 light_dir = light.pos.xyz - pos_frag;
	float light_dist = length(light_dir);
	light_dir /= light_dist;

	float attenuation = clamp(1.0 / (light.factors.x + light_dist*light.factors.y + light_dist*light_dist*light.factors.z)-0.01, 0.0, 1.0);

	if(!fast_lighting) {
		const float PI = 3.141;
		float theta = atan(light_dir.y, -light_dir.x)-light.dir;
		theta = ((theta/(2.0*PI)) - floor(theta/(2.0*PI))) * 2.0*PI - PI;

		attenuation *= my_smoothstep(-0.2, 0.6, clamp(light.angle-abs(theta), -1.0, 1.0));
	}

	return calc_light(light_dir, light.color, normal, albedo, view_dir, roughness, metalness, reflectance) * attenuation;
}
vec3 calc_dir_light(Dir_light light, vec3 normal, vec3 albedo, vec3 view_dir, float roughness, float metalness, float reflectance) {
	return calc_light(light.dir, light.color, normal, albedo, view_dir, roughness, metalness, reflectance);
}
