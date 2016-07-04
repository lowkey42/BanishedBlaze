#define GLM_SWIZZLE

#include "sprite_batch.hpp"

#include "command_queue.hpp"

namespace lux {
namespace renderer {

	using namespace glm;

	namespace {
		std::unique_ptr<Shader_program> sprite_shader;
		const auto def_uv_clip = glm::vec4{0,0,1,1};
		const std::vector<Sprite_vertex> single_sprite_vert {
			Sprite_vertex{{-0.5f,-0.5f, 0.f}, {0,1}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr},
			Sprite_vertex{{-0.5f,+0.5f, 0.f}, {0,0}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr},
			Sprite_vertex{{+0.5f,+0.5f, 0.f}, {1,0}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr},

			Sprite_vertex{{+0.5f,+0.5f, 0.f}, {1,0}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr},
			Sprite_vertex{{-0.5f,-0.5f, 0.f}, {0,1}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr},
			Sprite_vertex{{+0.5f,-0.5f, 0.f}, {1,1}, def_uv_clip, {1,0}, {0,0}, 0, 0.f, nullptr}
		};
	}

	Vertex_layout sprite_layout {
		Vertex_layout::Mode::triangles,
		vertex("position",  &Sprite_vertex::position),
		vertex("uv",        &Sprite_vertex::uv),
		vertex("uv_clip",   &Sprite_vertex::uv_clip),
		vertex("hue_change",&Sprite_vertex::hue_change),
		vertex("tangent",   &Sprite_vertex::tangent),
		vertex("shadow_resistence", &Sprite_vertex::shadow_resistence),
		vertex("decals_intensity", &Sprite_vertex::decals_intensity)
	};

	Sprite::Sprite(glm::vec3 position, Angle rotation, glm::vec2 size,
	               glm::vec4 uv, float shadow_resistence, float decals_intensity,
	               const renderer::Material& material)noexcept
	    : position(position), rotation(rotation), size(size),
	      uv(uv), shadow_resistence(shadow_resistence), decals_intensity(decals_intensity),
	      material(&material) {
	}

	Sprite_vertex::Sprite_vertex(glm::vec3 pos, glm::vec2 uv_coords, glm::vec4 uv_clip,
	                             glm::vec2 tangent, glm::vec2 hue_change, float shadow_resistence,
	                             float decals_intensity, const renderer::Material* material)
	    : position(pos), uv(uv_coords), uv_clip(uv_clip), tangent(tangent), hue_change(hue_change),
	      shadow_resistence(shadow_resistence), decals_intensity(decals_intensity), material(material) {
	}


	void init_sprite_renderer(asset::Asset_manager& asset_manager) {
		sprite_shader = std::make_unique<Shader_program>();
		sprite_shader->attach_shader(asset_manager.load<Shader>("vert_shader:sprite"_aid))
		              .attach_shader(asset_manager.load<Shader>("frag_shader:sprite"_aid))
		              .bind_all_attribute_locations(sprite_layout)
		              .build()
		              .uniforms(make_uniform_map(
		                  "albedo_tex", int(Texture_unit::color),
		                  "normal_tex", int(Texture_unit::normal),
		                  "material_tex", int(Texture_unit::material),
		                  "height_tex", int(Texture_unit::height),
		                  "shadowmaps_tex", int(Texture_unit::shadowmaps),
		                  "environment_tex", int(Texture_unit::environment),
		                  "last_frame_tex", int(Texture_unit::last_frame),
		                  "decals_tex", int(Texture_unit::decals)
		              ));
	}

	Sprite_batch::Sprite_batch(std::size_t expected_size)
	    : Sprite_batch(*sprite_shader, expected_size) {
	}
	Sprite_batch::Sprite_batch(Shader_program& shader, std::size_t expected_size)
	    : _shader(shader) {

		_vertices.reserve(expected_size*4);
		_objects.reserve(expected_size*0.25f);
	}

	auto Sprite_batch::_reserve_space(float z, const renderer::Material* material,
	                                  std::size_t count) -> Vertex_iter {
		// reserve enough space (first operation because of possible iterator invalidation)
		auto initial_size = _vertices.size();
		if(_vertices.capacity() < initial_size+count) {
			constexpr auto extra_space = 16;
			_vertices.reserve(initial_size + count + extra_space);
		}

		// locate the insertion point
		auto ip = std::lower_bound(_vertices.begin(), _vertices.end(), std::tie(z,material));
		auto ip_end = _vertices.end();
		auto end_len = static_cast<std::size_t>(std::distance(ip,ip_end));
		/* // FIXME: produces wrong results compared with the simpler/slower solution
		if(end_len < count) {
			// make enough room between ip and the new position of *ip for all new elements
			_vertices.resize(_vertices.size() + (count-end_len));
		}

		auto begin_insert_part = end_len>=count ? ip+(end_len-count) : ip_end;

		std::move(begin_insert_part, ip_end, std::back_inserter(_vertices));
		std::move_backward(ip, begin_insert_part, ip_end);

		*/
		_vertices.resize(_vertices.size() + count);
		std::move_backward(ip, ip_end, _vertices.end());


		INVARIANT(_vertices.size()==initial_size+count,
		          "Resize error. "<<_vertices.size()<<" instead of "<<initial_size<<"+"<<count<<" with end_len="<<end_len);

		return ip;
	}
	void Sprite_batch::insert(const Sprite& sprite) {
		auto scale = vec3 {
			sprite.size.x,
			sprite.size.y,
			1.f
		};

		auto transform = [&](vec3 p) {
			return sprite.position + rotate(p*scale, sprite.rotation, vec3{0,0,1});
		};

		auto tangent = rotate(vec3(1,0,0), sprite.rotation, vec3{0,0,1}).xy();

		auto tex_clip = sprite.material->albedo().clip_rect();
		auto sprite_clip = sprite.uv;

		// rescale uv to texture clip_rect
		sprite_clip.x *= (tex_clip.z - tex_clip.x);
		sprite_clip.z *= (tex_clip.z - tex_clip.x);
		sprite_clip.y *= (tex_clip.w - tex_clip.y);
		sprite_clip.w *= (tex_clip.w - tex_clip.y);
		// move uv by clip_rect offset
		sprite_clip.x += tex_clip.x;
		sprite_clip.z += tex_clip.x;
		sprite_clip.y += tex_clip.y;
		sprite_clip.w += tex_clip.y;

		sprite_clip.x += 0.5f / sprite.material->albedo().width();
		sprite_clip.y += 0.5f / sprite.material->albedo().height();
		sprite_clip.z -= 0.5f / sprite.material->albedo().width();
		sprite_clip.w -= 0.5f / sprite.material->albedo().height();

		auto iter = _reserve_space(sprite.position.z, sprite.material, single_sprite_vert.size());

		for(auto& vert : single_sprite_vert) {
			*iter = Sprite_vertex{transform(vert.position), vert.uv, sprite_clip,
			                      tangent, sprite.hue_change, sprite.shadow_resistence,
			                      sprite.decals_intensity, sprite.material};
			iter++;
		}
	}
	void Sprite_batch::insert(glm::vec3 position,
	                          const std::vector<Sprite_vertex>& vertices) {
		if(vertices.empty())
			return;

		auto begin = _reserve_space(position.z, vertices.front().material, vertices.size());
		auto iter = begin;

		for(auto& v : vertices) {
			*iter = Sprite_vertex{v.position + position,
			                      v.uv, v.uv_clip, v.tangent, v.hue_change,
			                      v.shadow_resistence, v.decals_intensity, v.material};
			iter++;
		}

#ifdef NDEBUG
		INVARIANT(std::is_sorted(vertices.begin(), vertices.end()), "inserted vertices nt sorted");
#endif
	}

	void Sprite_batch::flush(Command_queue& queue) {
		_draw(queue);
		_vertices.clear();
		_free_obj = 0;
	}

	void Sprite_batch::_draw(Command_queue& queue) {
		// draw one batch for each partition
		auto last = _vertices.begin();
		for(auto current = _vertices.begin(); current!=_vertices.end(); ++current) {
			if(current->material != last->material) {
				queue.push_back(_draw_part(last, current));
				last = current;
			}
		}

		if(last!=_vertices.end())
			queue.push_back(_draw_part(last, _vertices.end()));
/*
		if(!_vertices.empty()) {
			DEBUG("draw");
			float lastz = std::floor(_vertices.front().position.z*1000.f);
			bool last_alpha = _vertices.front().material->alpha();
			for(auto& v : _vertices) {
				auto z = std::floor(v.position.z*1000.f);
				if(last_alpha)
					INVARIANT(v.material->alpha(), "sort broken");

				DEBUG("   "<<z);
				if(!v.material->alpha()) {
				//	INVARIANT(v.position.z <= lastz, "sort broken");
				} else if(last_alpha == v.material->alpha()) {
					INVARIANT(z >= lastz, "sort broken");
				}

				last_alpha = v.material->alpha();
				lastz = z;
			}
		}
*/
	}

	auto Sprite_batch::_draw_part(Vertex_citer begin, Vertex_citer end) -> Command {
		INVARIANT(begin!=end, "Invalid iterators");
		INVARIANT(begin->material, "Invalid material");

		auto obj_idx = _free_obj++;

		if(obj_idx>=_objects.size()) {
			_objects.emplace_back(sprite_layout, create_buffer<Sprite_vertex>(begin, end, true));
		} else {
			_objects.at(obj_idx).buffer().set<Sprite_vertex>(begin, end);
		}

		auto cmd = create_command()
		        .shader(_shader)
		        .object(_objects.at(obj_idx));

		cmd.uniforms().emplace("alpha_cutoff",begin->material->alpha() ? 1.f/255 : 0.9f);

		begin->material->set_textures(cmd);

		cmd.uniforms().emplace("model", glm::mat4());

		return cmd;
	}

}
}
