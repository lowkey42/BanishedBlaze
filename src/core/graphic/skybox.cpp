#include "skybox.hpp"


namespace lux {
namespace graphic {

	using namespace glm;
	using namespace unit_literals;

	namespace {

		struct Sky_vertex {
			glm::vec3 pos;
		};

		Vertex_layout sky_vertex_layout {
			Vertex_layout::Mode::triangles,
			vertex("position", &Sky_vertex::pos)
		};

		const std::vector<Sky_vertex> skybox_vertices {
			// front
			Sky_vertex{{-1,-1,-1}},
			Sky_vertex{{-1,-1, 1}},
			Sky_vertex{{ 1,-1,-1}},
			Sky_vertex{{ 1,-1,-1}},
			Sky_vertex{{-1,-1, 1}},
			Sky_vertex{{ 1,-1, 1}},

			// back
			Sky_vertex{{-1, 1,-1}},
			Sky_vertex{{ 1, 1,-1}},
			Sky_vertex{{-1, 1, 1}},
			Sky_vertex{{ 1, 1,-1}},
			Sky_vertex{{ 1, 1, 1}},
			Sky_vertex{{-1, 1, 1}},

			// left
			Sky_vertex{{-1,-1,-1}},
			Sky_vertex{{-1, 1,-1}},
			Sky_vertex{{-1,-1, 1}},
			Sky_vertex{{-1, 1,-1}},
			Sky_vertex{{-1, 1, 1}},
			Sky_vertex{{-1,-1, 1}},

			// right
			Sky_vertex{{ 1,-1,-1}},
			Sky_vertex{{ 1,-1, 1}},
			Sky_vertex{{ 1, 1,-1}},
			Sky_vertex{{ 1, 1,-1}},
			Sky_vertex{{ 1,-1, 1}},
			Sky_vertex{{ 1, 1, 1}},

			// bottom
			Sky_vertex{{-1,-1,-1}},
			Sky_vertex{{ 1,-1,-1}},
			Sky_vertex{{-1, 1,-1}},
			Sky_vertex{{ 1,-1,-1}},
			Sky_vertex{{ 1, 1,-1}},
			Sky_vertex{{-1, 1,-1}},

			// top
			Sky_vertex{{-1,-1, 1}},
			Sky_vertex{{-1, 1, 1}},
			Sky_vertex{{ 1,-1, 1}},
			Sky_vertex{{ 1,-1, 1}},
			Sky_vertex{{-1, 1, 1}},
			Sky_vertex{{ 1, 1, 1}},
		};
	}

	Skybox::Skybox(asset::Asset_manager& assets)
	    : _obj(sky_vertex_layout, create_buffer(skybox_vertices)) {

		_prog
		        .attach_shader(assets.load<Shader>("vert_shader:skybox"_aid))
		        .attach_shader(assets.load<Shader>("frag_shader:skybox"_aid))
		        .bind_all_attribute_locations(sky_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "texture", int(Texture_unit::environment)
		        ));
	}

	void Skybox::draw(Command_queue& q)const {
		if(!_tex) {
			return;
		}

		if(_dirty) {
			_prog.bind().set_uniform("tint", _tint).set_uniform("brightness", _brightness);
			_dirty = false;
		}

		q.push_back(create_command()
		        .shader(_prog)
		        .texture(Texture_unit::environment, *_tex)
		        .object(_obj) );
	}

	void Skybox::texture(Texture_ptr tex) {
		_tex = std::move(tex);
		_tex->bind(int(Texture_unit::environment));
	}

}
}
