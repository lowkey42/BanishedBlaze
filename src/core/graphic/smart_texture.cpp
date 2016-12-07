#define GLM_SWIZZLE

#include "smart_texture.hpp"


namespace lux {
namespace graphic {

	Smart_texture::Smart_texture(Material_ptr material, std::vector<glm::vec2> points)
	    : _material(std::move(material)),
	      _points{points},
	      _dirty(true) {
	}

	auto Smart_texture::move_point(std::size_t i, glm::vec2 p) -> util::maybe<std::size_t> {
		_dirty = true;

		if(_points.size()>1) {
			if(glm::distance2(_points.at(i>0?i-1:_points.size()-1),p)<0.01) {
				_points.erase(_points.begin()+i);
				return i-1;
			}
			if(glm::distance2(_points.at((i+1) % _points.size()),p)<0.01) {
				_points.erase(_points.begin()+i);
				return i;
			}
		}

		_points.at(i) = p;
		return util::nothing();
	}

	void Smart_texture::insert_point(std::size_t i, glm::vec2 p) {
		_points.insert(_points.begin() + i, p);
		_dirty = true;
	}

	void Smart_texture::erase_point(std::size_t i) {
		_points.erase(_points.begin() + i);
		_dirty = true;
	}

	void Smart_texture::draw(glm::vec3 position, Sprite_batch& batch) {
		if(_dirty) {
			_update_vertices();
			_dirty = false;
		}

		batch.insert(position, _vertices);
	}

	namespace {
		auto cross(glm::vec2 v, glm::vec2 w) {
			return v.x*w.y - v.y*w.x;
		}
	}
	bool Smart_texture::is_inside(glm::vec2 my_pos, glm::vec2 point)const {
		point -= my_pos;

		int intersections = 0;

		for(auto i=0u; i<_points.size(); i++) {
			auto in = (i+1) % _points.size();
			auto p  = _points[i];
			auto pn = _points[in];
			auto pdiff = pn-p;

			auto s = glm::vec2{1000.f,0.f};
			auto cross_ds = cross(pdiff, s);
			auto t = cross(point-p, s/cross_ds);
			auto u = cross(point-p, pdiff/cross_ds);

			if(t>=0.f && t<=1.f && u>=0.f && u<=1.f) {
				intersections++;
			}
		}

		return (intersections % 2) == 1;
	}

	auto Smart_texture::get_point(glm::vec2 my_pos,
	                              glm::vec2 point,
	                              float point_size)const -> std::tuple<Point_location,std::size_t> {
		auto point_size2 = point_size*point_size;
		point-=my_pos;

		for(auto i=0u; i<_points.size(); i++) {
			auto p  = _points[i];

			auto dist2 = glm::length2(p-point);
			if(dist2<=point_size2) {
				return std::make_tuple(Point_location::on, i);
			}
		}

		point_size/=2.f;
		point_size2 = point_size*point_size;
		for(auto i=0u; i<_points.size(); i++) {
			auto in = (i+1) % _points.size();
			auto p  = _points[i];
			auto pn = _points[in];

			auto dist2 = glm::length2((p+(pn-p)/2.f)-point);
			if(dist2<=point_size2) {
				return std::make_tuple(Point_location::between, in);
			}
		}

		return std::make_tuple(Point_location::none, 0);
	}

	namespace {
		using namespace glm;
		using namespace unit_literals;

		auto is_convex(vec2 a, vec2 b, vec2 c) {
			return ( ( a.x * ( c.y - b.y ) ) + ( b.x * ( a.y - c.y ) ) + ( c.x * ( b.y - a.y ) ) ) < 0.f;
		}
		auto sign(vec2 p1, vec2 p2, vec2 p3){
			return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
		}

		auto is_in_triangle(vec2 pt, vec2 v1, vec2 v2, vec2 v3){
			auto b1 = sign(pt, v1, v2) < 0.0f;
			auto b2 = sign(pt, v2, v3) < 0.0f;
			auto b3 = sign(pt, v3, v1) < 0.0f;

			return b1 == b2 && b2 == b3;
		}
		auto is_ear(std::size_t a, std::size_t b, std::size_t c, const std::vector<vec2>& points) {
			auto prev = points[a];
			auto curr = points[b];
			auto next = points[c];

			for(auto i=0u; i<points.size(); i++) {
				if(i!=a && i!=b && i!=c && is_in_triangle(points[i], prev, curr, next)) {
					return false;
				}
			}

			return true;
		}

		template<typename F1, typename F2>
		void triangulate(std::vector<glm::vec2> points, F1&& add_vertex, F2&& error) {
			int triangles_produced = 0;
			do {
				triangles_produced = 0;

				for(auto i=0u; i<points.size() && points.size()>3; i++) {
					auto pi = i>0 ? i-1 : points.size()-1;
					auto ni = (i+1) % points.size();

					auto prev = points[pi];
					auto curr = points[i];
					auto next = points[ni];

					if(is_convex(prev, curr, next) && is_ear(pi,i,ni,points)) {
						points.erase(points.begin()+i);
						triangles_produced++;

						// create triangle (prev, curr, next)
						add_vertex(prev);
						add_vertex(curr);
						add_vertex(next);
					}
				}

			} while(points.size()>3 && triangles_produced>0);

			if(points.size()==3) {
				add_vertex(points[0]);
				add_vertex(points[1]);
				add_vertex(points[2]);

			} else {
				error(points.size());
			}
		}

		void triangulate_background(const std::vector<glm::vec2>& points,
		                            std::vector<Sprite_vertex>& vertices,
		                            bool shadowcaster, float decals_intensity,
		                            const graphic::Material& mat) {

			const auto pc = 2.0f / mat.albedo().width();

			auto add_vertex = [&](auto v) {
				auto uv_clip = vec4{pc, pc, 0.75f-pc, 0.75f-pc};
				auto uv = vec2{v.x,-v.y}*0.5f;
				auto hc = glm::vec2{0,0}; // hue_change. currently unused by smart_textures

				vertices.emplace_back(vec3(v,-0.04f), vec2{}, uv, uv_clip, vec2{1.f,0.f}, hc,
				                      shadowcaster ? 1.f : 0.f, decals_intensity, &mat);
			};
			auto error = [&](auto left) {
				INFO("Polygon is not valid. "<<left<<" vertices left");
				vertices.clear();
			};

			triangulate(points, add_vertex, error);
		}
		void triangulate_border(const std::vector<glm::vec2>& points,
		                        std::vector<Sprite_vertex>& vertices,
		                        bool shadowcaster, float decals_intensity,
		                        const graphic::Material& mat) {

			constexpr auto scale = 1.f;

			const auto pc = 0.5f / mat.albedo().width();

			constexpr auto h = 0.5f * scale;
			constexpr auto hh = h / 2.f;
			constexpr auto connection_limit = 0.5f;

			auto shadow_res = shadowcaster ? 1.f : 0.f;

			auto prev_i = [&](auto i) {
				return i>0 ? i-1 : points.size()-1;
			};
			auto next_i = [&](auto i) {
				return (i+1) % points.size();
			};

			auto calc_normal = [&](auto pi, auto i) {
				auto diff = points[i] - points[pi];
				return glm::normalize(vec2{-diff.y, diff.x});
			};

			auto angle_between = [&](auto v1, auto v2) {
				auto a = glm::atan(v2.y,v2.x) - glm::atan(v1.y,v1.x);
				if(a<0.f)
					a+=2.f*glm::pi<float>();

				return Angle{a};
			};

			std::vector<Sprite_vertex> vertex_tmp_buffer;

			auto add_sprite_points = [&](float d, vec2 tangent, vec2 tl, vec2 tr, vec2 bl, vec2 br,
			                             vec4 uv_clip,
			                             vec2 uv_tl, vec2 uv_tr, vec2 uv_bl, vec2 uv_br, bool later=false) {
				d= 0.0f;

				auto tex_clip = mat.albedo().clip_rect();
				// rescale uv to texture clip_rect
				uv_clip.x *= (tex_clip.z - tex_clip.x);
				uv_clip.z *= (tex_clip.z - tex_clip.x);
				uv_clip.y *= (tex_clip.w - tex_clip.y);
				uv_clip.w *= (tex_clip.w - tex_clip.y);
				// move uv by clip_rect offset
				uv_clip.x += tex_clip.x;
				uv_clip.z += tex_clip.x;
				uv_clip.y += tex_clip.y;
				uv_clip.w += tex_clip.y;

				uv_clip.x += 1.0f / mat.albedo().width();
				uv_clip.y += 1.0f / mat.albedo().height();
				uv_clip.z -= 1.0f / mat.albedo().width();
				uv_clip.w -= 1.0f / mat.albedo().height();

				auto hc = glm::vec2{0,0}; // hue_change. currently unused by smart_textures

				auto& v = later ? vertex_tmp_buffer : vertices;

				v.emplace_back(vec3(bl, d), vec2{}, uv_bl, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);
				v.emplace_back(vec3(tl, d), vec2{}, uv_tl, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);
				v.emplace_back(vec3(tr, d), vec2{}, uv_tr, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);

				v.emplace_back(vec3(tr, d), vec2{}, uv_tr, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);
				v.emplace_back(vec3(bl, d), vec2{}, uv_bl, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);
				v.emplace_back(vec3(br, d), vec2{}, uv_br, uv_clip, tangent, hc, shadow_res, decals_intensity, &mat);
			};

			auto add_sprite = [&](float d, vec2 left, vec2 right,
			                      vec2 normal_l, vec2 normal_r, vec4 uv_clip,
			                      vec2 uv_tl, vec2 uv_tr, vec2 uv_bl, vec2 uv_br, bool later=false) {

				auto tangent = glm::normalize(right - left);

				auto top_left = left + normal_l*hh;
				auto top_right = right + normal_r*hh;
				auto bottom_left = left - normal_l*hh;
				auto bottom_right = right - normal_r*hh;

				add_sprite_points(d, tangent, top_left, top_right, bottom_left, bottom_right,
				                  uv_clip, uv_tl, uv_tr, uv_bl, uv_br, later);
			};

			auto i_offset = 0u;
			for(;i_offset<points.size(); i_offset++) {
				auto pi = prev_i(i_offset);
				auto normal = calc_normal(pi, i_offset);
				auto normal_prev = calc_normal(prev_i(pi), pi);

				if(glm::length2(normal-normal_prev)>connection_limit) {
					break;
				}
			}
			i_offset = i_offset % points.size();


			auto w = 0.f;
			for(auto j=0u; j<points.size(); j++) {
				auto i = (j+i_offset) % points.size();
				auto pi = prev_i(i);

				auto prev = points[pi];
				auto curr = points[i];
				auto next = points[next_i(i)];

				auto edge_offset_left = 0.f;
				auto edge_offset_right = 0.f;

				auto normal = calc_normal(pi, i);
				auto tangent = glm::normalize(points[i] - points[pi]);
				auto normal_prev = calc_normal(prev_i(pi), pi);
				auto tangent_prev = glm::normalize(points[pi] - points[prev_i(pi)]);
				auto normal_next = calc_normal(i, next_i(i));
				auto tangent_next = glm::normalize(points[next_i(i)] - points[i]);
				auto normal_l = normal;
				auto normal_r = normal;

				auto vertical = glm::abs(normal.x) >= glm::abs(normal.y);
				auto vertical_prev = glm::abs(normal_prev.x) >= glm::abs(normal_prev.y);
				auto vertical_next = glm::abs(normal_next.x) >= glm::abs(normal_next.y);

				auto angle_left = angle_between(prev-points[prev_i(pi)], prev-curr);
				auto angle_right = angle_between(prev-curr, next-curr);

				auto has_edge_left = abs(angle_left-180_deg)>=45_deg || vertical!=vertical_prev;
				auto has_edge_right = abs(angle_right-180_deg)>45_deg || vertical!=vertical_next;

				if(!has_edge_left || (vertical && vertical_prev)) {
					normal_l = glm::normalize(glm::mix(normal_prev, normal, 0.5f));
				} else {
					// skip for extremly sharp edges
					if(angle_left<=360_deg-45_deg) {
						// solve the contact-equation for this edge: x*T_1 +- h/2*N_1 = -x*T_2 +- h/2*N_2
						auto x = (h*(normal-normal_prev) / (2.f*(tangent_prev+tangent))).x;
						x = std::max(x, (h*(normal_prev-normal) / (2.f*(tangent_prev+tangent))).x);
						prev+=tangent * x;
						edge_offset_left = x;
					} else {
						has_edge_left = false;
						normal_l = glm::normalize(glm::mix(normal_prev, normal, 0.5f));
					}

					w = 0.f;
				}

				if(!has_edge_right || (vertical && vertical_next)) {
					normal_r = glm::normalize(glm::mix(normal, normal_next, 0.5f));
				} else {
					// skip for extremly sharp edges
					if(angle_right<=360_deg-45_deg) {
						// solve the contact-equation for this edge: x*T_1 +- h/2*N_1 = -x*T_2 +- h/2*N_2
						auto x = (h*(normal_next-normal) / (2.f*(tangent+tangent_next))).x;
						x = std::max(x, (h*(normal-normal_next) / (2.f*(tangent+tangent_next))).x);
						curr-=tangent * x;
						edge_offset_right = x;
					} else {
						has_edge_right = false;
						normal_r = glm::normalize(glm::mix(normal, normal_next, 0.5f));
					}
				}

				auto s = w;
				w += glm::length(curr - prev)*0.5f / scale;

				if(vertical) { // vertical

					if(normal.x>0) { // left facing
						add_sprite(0.0f, prev, curr, normal_l, normal_r,
						           vec4{0.75f+pc,0.f, 0.875f-pc, 0.625f-pc},
						           {1,s}, {1,w}, {0,s}, {0,w});

					} else { // right facing
						add_sprite(0.0f, prev, curr, normal_l, normal_r,
						           vec4{0.875f+pc,0.f, 1.f, 0.625f-pc},
						           {0,-s}, {0,-w}, {1,-s}, {1,-w});
					}
				} else { // horizontal
					auto edge_y_offset = 0.f;


					if(normal.y>0) { // down facing
						edge_y_offset = 0.125f;
						add_sprite(0.0f, prev, curr, normal_l, normal_r,
						           vec4{0.125f+pc,0.875f+pc, 0.75f-pc, 1.f},
						           {s,0}, {w,0}, {s,1}, {w,1});

						// left edge
						if(has_edge_left) {
							if(angle_left>180_deg) { // edge is going upwards
								auto p1 = points[pi] - tangent_prev*edge_offset_left - normal_prev*hh;
								auto p2 = points[pi] - tangent_prev*edge_offset_left + normal_prev*hh;
								auto p3 = points[pi] - normal_prev*std::max(edge_offset_left,hh) - normal*std::max(edge_offset_left,hh);
								auto p4 = prev - normal*hh;
								add_sprite_points(0.f, tangent, p1, p2, p3, p4,
								                  vec4{0.875f,0.625f+pc, 1.f-pc, 0.75f-pc},
								                  {0,0}, {1,0}, {0,1}, {1,1}, true);
							} else {
								auto p1 = points[pi] + normal_prev*std::max(edge_offset_left,hh) + normal*std::max(edge_offset_left,hh);
								auto p2 = prev + normal*hh;
								auto p3 = points[pi] - tangent_prev*edge_offset_left + normal_prev*hh;
								auto p4 = prev - normal*hh;
								add_sprite_points(0.f, tangent, p1, p2, p3, p4,
								                  vec4{0.0f,0.75f+edge_y_offset+pc, 0.125f-pc, 0.875f+edge_y_offset-pc},
								                  {0,0}, {1,0}, {0,1}, {1,1}, true);
							}
						}

						// right edge
						if(has_edge_right) {
							if(angle_right>180_deg) { // edge is going upwards
								auto p1 = curr + normal*hh;
								auto p2 = p1 - normal_next*h;
								auto p3 = curr - normal*hh;
								auto p4 = points[i] - normal_next*std::max(edge_offset_right,hh) - normal*std::max(edge_offset_right,hh);
								add_sprite_points(0.f, tangent, p1, p2, p3, p4,
								                  vec4{0.875f,0.75f+edge_y_offset+pc, 1.0f-pc, 0.875f+edge_y_offset-pc},
								                  {0,0}, {1,0}, {0,1}, {1,1}, true);
							} else {
								auto p1 = curr + normal*hh;
								auto p2 = points[i] + normal_next*std::max(edge_offset_right,hh) + normal*std::max(edge_offset_right,hh);
								auto p3 = curr - normal*hh;
								auto p4 = p3 + normal_next*h;
								add_sprite_points(0.f, tangent, p1, p2, p3, p4,
								                  vec4{0.75f,0.75f+edge_y_offset+pc, 0.875f-pc, 0.875f+edge_y_offset-pc},
								                  {0,0}, {1,0}, {0,1}, {1,1}, true);
							}
						}

					} else { // up facing
						std::swap(has_edge_left, has_edge_right); // upside down
						std::swap(angle_left, angle_right); // upside down
						std::swap(edge_offset_left, edge_offset_right); // upside down

						edge_y_offset = 0.0f;
						add_sprite(0.0f, prev, curr, normal_l, normal_r,
						           vec4{0.125f+pc,0.75f+pc, 0.75f-pc, 0.875f-pc},
						           {-s,1}, {-w,1}, {-s,0}, {-w,0});

						// left edge
						if(has_edge_left) {
							if(angle_left>180_deg) { // edge is going downwards
								auto p1 = points[i] - normal_next*std::max(edge_offset_left,hh) - normal*std::max(edge_offset_left,hh);
								auto p2 = curr - normal*hh;
								auto p4 = curr + normal*hh;
								auto p3 = p4 - normal_next*h;
								add_sprite_points(0.f, tangent, p4, p3, p2, p1,
								                  vec4{0.0f,0.75f+edge_y_offset+pc, 0.125f-pc, 0.875f+edge_y_offset-pc},
								                  {1,1}, {0,1}, {1,0}, {0,0}, true);
							} else {
								auto p1 = points[i] + normal_next*std::max(edge_offset_left,hh) + normal*std::max(edge_offset_left,hh);
								auto p2 = curr + normal*hh;
								auto p4 = curr - normal*hh;
								auto p3 = p4 + normal_next*h;
								add_sprite_points(0.f, tangent, p2, p1, p4, p3,
								                  vec4{0.75f,0.625f+edge_y_offset+pc, 0.875f-pc, 0.75f+edge_y_offset-pc},
								                  {1,1}, {0,1}, {1,0}, {0,0}, true);
							}
						}

						// right edge
						if(has_edge_right) {
							if(angle_right>180_deg) { // edge is going downwards
								auto p1 = points[pi] - normal_prev*std::max(edge_offset_right,hh) - normal*std::max(edge_offset_right,hh);
								auto p2 = prev - normal*hh;
								auto p4 = prev + normal*hh;
								auto p3 = p4 - normal_prev*h;
								add_sprite_points(0.f, tangent, p3, p4, p1, p2,
								                  vec4{0.75f,0.75f+edge_y_offset+pc, 0.875f-pc, 0.875f+edge_y_offset-pc},
								                  {1,1}, {0,1}, {1,0}, {0,0}, true);

							} else {
								auto p1 = points[pi] + normal_prev*std::max(edge_offset_right,hh) + normal*std::max(edge_offset_right,hh);
								auto p2 = prev + normal*hh;
								auto p4 = prev - normal*hh;
								auto p3 = p4 + normal_prev*h;
								add_sprite_points(0.f, tangent, p1, p2, p3, p4,
								                  vec4{0.875f,0.75f+edge_y_offset+pc, 1.0f-pc, 0.875f+edge_y_offset-pc},
								                  {1,1}, {0,1}, {1,0}, {0,0}, true);
							}
						}

					}
				}
			}

			vertices.insert(vertices.end(), vertex_tmp_buffer.begin(), vertex_tmp_buffer.end());
		}
	}
	void Smart_texture::_update_vertices() {
		_vertices.clear();

		triangulate_background(_points, _vertices, _shadowcaster, _decals_intensity, *_material);
		triangulate_border(_points, _vertices, _shadowcaster, _decals_intensity, *_material);
	}

	auto Smart_texture::vertices()const -> std::vector<glm::vec2> {
		auto ret = std::vector<glm::vec2>();
		ret.reserve(_points.size());

		auto add_vertex = [&](auto v) {
			ret.emplace_back(v);
		};
		auto error = [&](auto left) {
			INFO("Polygon is not valid. "<<left<<" vertices left");
		};

		triangulate(_points, add_vertex, error);

		return ret;
	}
}
}
