
#include <_uniforms_lighting.glsl>

uniform sampler2D shadowmap_0_tex;
uniform sampler2D shadowmap_1_tex;
uniform sampler2D shadowmap_2_tex;
uniform sampler2D shadowmap_3_tex;


vec3 calc_dir_light(vec3 diff_color, vec3 F0, vec3 N, vec3 V, float roughness);
vec3 calc_env_light(vec3 diff_color, vec3 F0, vec3 N, vec3 V, float roughness);
vec3 calc_point_light(vec3 position, vec3 diff_color, vec3 F0, vec3 N, vec3 V,
                      float roughness, int idx);

vec3 calc_light(vec3 position, vec3 diff_color, vec3 F0, vec3 N, vec3 V, float roughness) {
	vec3 color = calc_env_light(diff_color, F0, N, V, roughness);
	color += calc_dir_light(diff_color, F0, N, V, roughness);

	color += calc_point_light(position, diff_color, F0, N, V, roughness, 0)
	        * texture(shadowmap_0_tex, input.shadowmap_uv).rgb;

	color += calc_point_light(position, diff_color, F0, N, V, roughness, 1)
	        * texture(shadowmap_1_tex, input.shadowmap_uv).rgb;

	color += calc_point_light(position, diff_color, F0, N, V, roughness, 2)
	        * texture(shadowmap_2_tex, input.shadowmap_uv).rgb;

	color += calc_point_light(position, diff_color, F0, N, V, roughness, 3)
	        * texture(shadowmap_3_tex, input.shadowmap_uv).rgb;

	return color;
}


vec3 calc_generic_light(vec3 light_color, vec3 diff_color, vec3 F0,
                        float roughness, vec3 N, vec3 V, vec3 L);
vec4 spec_cook_torrence(vec3 F0, float roughness,
                        vec3 N, vec3 V, vec3 L, float NdotL);

vec3 calc_dir_light(vec3 diff_color, vec3 F0, vec3 N, vec3 V, float roughness) {
	return calc_generic_light(directional_light.rgb,
	                          diff_color, F0, roughness,
	                          N, V, directional_dir.xyz);
}

vec3 calc_env_light(vec3 diff_color, vec3 F0, vec3 N, vec3 V, float roughness) {
	// TODO
	return diff_color * ambient_light.rgb;
}

vec3 calc_point_light(vec3 position, vec3 diff_color, vec3 F0, vec3 N, vec3 V,
                      float roughness, int idx) {
	vec3 L = light[idx].pos.xyz - position;
	float light_dist = length(L);
	L /= light_dist;

	light_dist = max(0.0, light_dist-light[idx].src_radius);


	float attenuation = clamp(1.0 / max(1.0, light_dist*light_dist), 0.0, 1.0);

	return calc_generic_light(light[idx].color.rgb,
	                          diff_color, F0, roughness,
	                          N, V, L) * attenuation;
}


vec3 calc_generic_light(vec3 light_color, vec3 diff_color, vec3 F0,
                        float roughness, vec3 N, vec3 V, vec3 L) {

	float NdotL = max(0.0, dot(N, L));
	vec4 spec_ct = spec_cook_torrence(F0, roughness, N,V,L,NdotL);

	vec3 ks = spec_ct.rgb;
	vec3 kd = vec3(1.0) - ks;
	vec3 spec = light_color * spec_ct.a * ks * step(0.01, NdotL);
	vec3 diff = light_color * NdotL * diff_color * kd;

	return (spec + diff);
}

// TODO: fix black "highlight" if light and camera are close to the surface
vec4 spec_cook_torrence(vec3 F0, float roughness,
                        vec3 N, vec3 L, vec3 V, float NdotL) {

	const float PI = 3.14159265359;

	vec3 H = normalize(L + V);

	float HdotV = max(0.0, dot(H, V));
	float HdotL = max(0.0, dot(H, L));

	float NdotV = max(0.0, dot(N, V));
	float NdotH = max(0.0, dot(N, H));

	float NdotH_2 = NdotH * NdotH;
	float NdotH_4 = NdotH_2 * NdotH_2;

	// fresnel term, schlick's  appox.
	vec3 F = F0 + (1.0-F0) * pow(1.0 - HdotL, 5);

	// distribution function, GGX
	float alpha = roughness * roughness;
	float alpha_2 = alpha * alpha;
	float denom = NdotH*NdotH *  (alpha_2 - 1.0) + 1.0;
	float D = alpha_2 / (PI * denom * denom);

	// geometry factor, GGX
	float chi = (HdotV / NdotV) > 0.0 ? 1.0 : 0.0;
	float HdotV_2 = HdotV * HdotV;
	float tan2 = ( 1 - HdotV_2 ) / HdotV_2;
	float G = (chi * 2) / (1 + sqrt(1 + roughness*roughness * tan2));

	float spec = max(0.0, (D*G) / (4*NdotL*NdotV + 0.05));

	return vec4(F, spec);
}
