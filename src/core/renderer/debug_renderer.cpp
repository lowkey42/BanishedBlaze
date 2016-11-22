#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "debug_renderer.hpp"

#include "camera.hpp"
#include "command_queue.hpp"
#include "graphics_ctx.hpp"
#include "vertex_object.hpp"
#include "texture.hpp"
#include "framebuffer.hpp"
#include "shader.hpp"
#include "primitives.hpp"

#include "../asset/asset_manager.hpp"

#include <SDL2/SDL.h>

#include <glm/glm.hpp>


namespace lux {
namespace renderer {

	namespace {
		Vertex_layout debug_layout {
			Vertex_layout::Mode::triangles,
			vertex("position",  &Simple_vertex::xy),
			vertex("uv",        &Simple_vertex::uv)
		};

		std::unique_ptr<Shader_program> debug_shader;

		std::unique_ptr<Object> debug_quad;

		const auto quad_vertices = std::vector<Simple_vertex> {
			Simple_vertex{{-0.5f,-0.5f}, {0,1}},
			Simple_vertex{{-0.5f,+0.5f}, {0,0}},
			Simple_vertex{{+0.5f,+0.5f}, {1,0}},

			Simple_vertex{{+0.5f,+0.5f}, {1,0}},
			Simple_vertex{{-0.5f,-0.5f}, {0,1}},
			Simple_vertex{{+0.5f,-0.5f}, {1,1}}
		};

		Command_queue debug_queue;
		float next_x = 0.f;
		float quad_y = 0.f;
		float quad_width = 200.f;
	}

	void init_debug_renderer(asset::Asset_manager& asset_manager, Graphics_ctx& ctx) {
		auto size = glm::vec2{ctx.win_width(),ctx.win_height()};
		auto cam = Camera_2d(ctx.viewport(), size);
		cam.position(size/2.f);
		auto vp = cam.vp();

		quad_width = cam.size().x/8;
		quad_y = cam.size().y;
		next_x = quad_width/2.f;

		debug_shader = std::make_unique<Shader_program>();
		debug_shader->attach_shader(asset_manager.load<Shader>("vert_shader:debug"_aid))
		             .attach_shader(asset_manager.load<Shader>("frag_shader:debug"_aid))
		             .bind_all_attribute_locations(debug_layout)
		             .build()
		             .uniforms(make_uniform_map(
		                 "texture", int(Texture_unit::temporary),
		                 "vp", vp,
		                 "model", glm::mat4(),
		                 "clip", glm::vec4{0,0,1,1},
		                 "color", glm::vec4{1,1,1,1}
		             ));

		debug_quad = std::make_unique<Object>(debug_layout, create_buffer(quad_vertices));
	}

	void debug_draw_framebuffer(Framebuffer& fb) {
		for(auto& attachment : fb._attachments) {
			debug_draw_texture(std::get<1>(attachment.second));
		}
	}

	void debug_draw_texture(Texture& tex) {
		auto model = glm::mat4();
		model[0][0] = quad_width;
		model[1][1] = tex.height()*quad_width/tex.width();
		if(model[1][1]<10)
			model[1][1] = quad_width;
		model[3][0] = next_x; next_x+=quad_width;
		model[3][1] = quad_y - model[1][1]/2.f;

		auto cmd = create_command();
		cmd.object(*debug_quad);
		cmd.require_not(Gl_option::depth_test);
		cmd.require_not(Gl_option::depth_write);
		cmd.shader(*debug_shader);
		cmd.texture(Texture_unit::temporary, tex);
		cmd.uniforms().emplace("model", model);
		debug_queue.push_back(std::move(cmd));
	}

	void on_frame_debug_draw() {
		debug_queue.flush();
		next_x = quad_width/2.f;
	}

} /* namespace renderer */
}
