#define GLM_SWIZZLE

#include "texture_batch.hpp"

#include "command_queue.hpp"
#include "primitives.hpp"


namespace lux {
namespace renderer {

	using namespace glm;

	namespace {
		Vertex_layout tex_layout {
			Vertex_layout::Mode::triangles,
			vertex("position",  &Texture_Vertex::position),
			vertex("uv",        &Texture_Vertex::uv)
		};

		std::unique_ptr<Shader_program> tex_shader;

		std::unique_ptr<Object> single_quat_tex;

		const auto single_tex_vert = std::vector<Simple_vertex> {
			Simple_vertex{{-0.5f,-0.5f}, {0,0}},
			Simple_vertex{{-0.5f,+0.5f}, {0,1}},
			Simple_vertex{{+0.5f,+0.5f}, {1,1}},

			Simple_vertex{{+0.5f,+0.5f}, {1,1}},
			Simple_vertex{{-0.5f,-0.5f}, {0,0}},
			Simple_vertex{{+0.5f,-0.5f}, {1,0}}
		};
	}

	Texture_Vertex::Texture_Vertex(glm::vec2 pos, glm::vec2 uv_coords,
	                             const renderer::Texture* texture)
	    : position(pos), uv(uv_coords), tex(texture) {
	}


	void init_texture_renderer(asset::Asset_manager& asset_manager) {
		tex_shader = std::make_unique<Shader_program>();
		tex_shader->attach_shader(asset_manager.load<Shader>("vert_shader:simple"_aid))
		           .attach_shader(asset_manager.load<Shader>("frag_shader:simple"_aid))
		           .bind_all_attribute_locations(tex_layout)
		           .build()
		           .uniforms(make_uniform_map(
		               "texture", int(Texture_unit::color),
		               "model", glm::mat4(),
		               "clip", glm::vec4{0,0,1,1},
		               "color", glm::vec4{1,1,1,1}
		           ));

		single_quat_tex = std::make_unique<Object>(simple_vertex_layout, create_buffer(single_tex_vert));
	}

	void draw_fullscreen_quad(const renderer::Texture& tex, Texture_unit unit) {
		tex.bind(static_cast<int>(unit));
		single_quat_tex->draw();
	}

	Texture_batch::Texture_batch(std::size_t expected_size,
	                             bool depth_test)
	    : _depth_test(depth_test) {
		_vertices.reserve(expected_size*4);
		_objects.reserve(expected_size*0.25f);
	}

	void Texture_batch::insert(const Texture& texture, glm::vec2 pos, glm::vec2 size,
	                           Angle rotation, glm::vec4 clip_rect) {

		if(size.x<0.0f) size.x = texture.width() * (clip_rect.z-clip_rect.x);
		if(size.y<0.0f) size.y = texture.height() * (clip_rect.w-clip_rect.y);

		auto scale = vec2 {
			size.x,
			size.y
		};

		auto transform = [&](vec2 p) {
			return pos + rotate(p*scale, rotation);
		};

		auto tex_clip = texture.clip_rect();
		auto sprite_clip = clip_rect;

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

		sprite_clip.x += 1.0f / texture.width();
		sprite_clip.y += 1.0f / texture.height();
		sprite_clip.z -= 1.0f / texture.width();
		sprite_clip.w -= 1.0f / texture.height();

		auto uv_size = sprite_clip.zw() - sprite_clip.xy();

		for(auto& vert : single_tex_vert) {
			auto uv = sprite_clip.xy() + uv_size * vert.uv;
			_vertices.emplace_back(transform(vert.xy), uv, &texture);
		}
	}

	void Texture_batch::flush(Command_queue& queue) {
		_draw(queue);
		_vertices.clear();
		_free_obj = 0;
	}

	void Texture_batch::_draw(Command_queue& queue) {
		// partition _vertices by texture
		//std::stable_sort(_vertices.begin(), _vertices.end());

		_reserve_objects();

		// draw one batch for each partition
		auto last = _vertices.begin();
		for(auto current = _vertices.begin(); current!=_vertices.end(); ++current) {
			if(current->tex != last->tex) {
				queue.push_back(_draw_part(last, current));
				last = current;
			}
		}

		if(last!=_vertices.end())
			queue.push_back(_draw_part(last, _vertices.end()));
	}

	auto Texture_batch::_draw_part(Vertex_citer begin, Vertex_citer end) -> Command {
		INVARIANT(begin!=end && begin->tex, "Invalid iterators");

		auto obj_idx = _free_obj++;

		INVARIANT(obj_idx<_objects.size(), "Too few objects reserved");

		_objects.at(obj_idx).buffer().set<Texture_Vertex>(begin, end);

		auto cmd = create_command()
		        .shader(*tex_shader)
		        .require_not(Gl_option::depth_write)
		        .order_dependent()
		        .texture(Texture_unit::color, *begin->tex)
		        .object(_objects.at(obj_idx));

		if(!_depth_test)
			cmd.require_not(Gl_option::depth_test);

		cmd.uniforms().emplace("layer", _layer);

		return cmd;
	}

	void Texture_batch::_reserve_objects() {
		// reserve required objects
		auto req_objs = 0u;
		auto last_tex = static_cast<const Texture*>(nullptr);
		for(auto& v : _vertices) {
			if(v.tex!=last_tex) {
				last_tex = v.tex;
				req_objs++;
			}
		}
		if(req_objs>_objects.size()) {
			_objects.reserve(req_objs);
			req_objs-=_objects.size();
			for(auto i=0u; i<req_objs; i++) {
				_objects.emplace_back(tex_layout, create_dynamic_buffer<Texture_Vertex>(8));
			}
		}
	}

}
}
