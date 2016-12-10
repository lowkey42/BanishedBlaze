#include "primitives.hpp"

#include "vertex_object.hpp"
#include "command_queue.hpp"

namespace lux {
namespace graphic {

	Vertex_layout simple_vertex_layout {
		Vertex_layout::Mode::triangles,
		vertex("position", &Simple_vertex::xy),
		vertex("uv", &Simple_vertex::uv)
	};

	namespace {
		struct Line_vertex {
			float index;
		};

		Vertex_layout line_vertex_layout {
			Vertex_layout::Mode::lines,
			vertex("index", &Line_vertex::index)
		};

		std::unique_ptr<Shader_program> dashed_line_shader;

		std::unique_ptr<Object> unit_line;
	}
	void init_primitives(asset::Asset_manager& asset_manager) {
		dashed_line_shader = std::make_unique<Shader_program>();
		dashed_line_shader->
		        attach_shader(asset_manager.load<Shader>("vert_shader:dashed_line"_aid))
		       .attach_shader(asset_manager.load<Shader>("frag_shader:dashed_line"_aid))
		       .bind_all_attribute_locations(line_vertex_layout)
		       .build()
		       .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		       .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting));

		std::vector<Line_vertex> unit_line_data {
			{0},
			{1}
		};
		unit_line = std::make_unique<Object>(line_vertex_layout, create_buffer(unit_line_data));
	}

	void draw_dashed_line(graphic::Command_queue& queue,
	                      glm::vec2 p1, glm::vec2 p2, float dash_len, Rgba color) {
		auto cmd = create_command()
		           .require_not(Gl_option::depth_write)
		           .order_dependent()
		           .object(*unit_line)
		           .shader(*dashed_line_shader);

		cmd.uniforms().emplace("p1", p1);
		cmd.uniforms().emplace("p2", p2);
		cmd.uniforms().emplace("dash_len", 1.f/dash_len);
		cmd.uniforms().emplace("color", color);

		queue.push_back(cmd);
	}

}
}
