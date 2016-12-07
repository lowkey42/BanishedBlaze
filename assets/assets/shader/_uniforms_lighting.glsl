
struct Point_light {
	vec4 pos;
	vec4 flat_pos;
	vec4 color;
	float direction;
	float angle;
	float area_of_effect;
	float src_radius;
};

layout(std140) uniform lighting {
	vec4 ambient_light;
	vec4 directional_light;
	vec4 directional_dir;
	Point_light light[4];
};
