
layout(std140) uniform globals {
	mat4 view;
	mat4 proj;
	mat4 vp;
	mat4 vp_inv;
	mat4 sse_vp;
	mat4 sse_vp_inv;
	vec4 eye;
};
