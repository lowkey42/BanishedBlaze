#include "renderer_effects.hpp"


namespace lux {
namespace sys {
namespace renderer {

	using namespace graphic;
	
	namespace {
		auto framebuffer_size(Engine& engine) {
			return glm::vec2{engine.graphics_ctx().settings().width,
			                 engine.graphics_ctx().settings().height}
			       * engine.graphics_ctx().settings().supersampling;
		}

		auto create_effect_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine);
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_16F);
			framebuffer.build();

			return framebuffer;
		}
		auto create_blur_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine) / 2.f;
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_16F);
			framebuffer.build();

			return framebuffer;
		}
	}
	
	Renderer_effects::Renderer_effects(Engine& engine) 
	    : _graphics_ctx(engine.graphics_ctx()),
	      _blur_canvas{create_blur_framebuffer(engine), create_blur_framebuffer(engine)},
	      _motion_blur_canvas(create_effect_framebuffer(engine)) {
		
		auto& assets = engine.assets();
		auto blur_size = framebuffer_size(engine) / 2.f;
		
		_blur_shader.attach_shader(assets.load<Shader>("vert_shader:blur"_aid))
		            .attach_shader(assets.load<Shader>("frag_shader:blur"_aid))
		            .bind_all_attribute_locations(simple_vertex_layout)
		            .build()
		            .uniforms(make_uniform_map(
		                 "tex", int(Texture_unit::temporary),
		                 "texture_size", blur_size
		             ));

		_motion_blur_shader
		           .attach_shader(assets.load<Shader>("vert_shader:motion_blur"_aid))
		           .attach_shader(assets.load<Shader>("frag_shader:motion_blur"_aid))
		           .bind_all_attribute_locations(simple_vertex_layout)
		           .build()
		           .uniforms(make_uniform_map(
		                "tex", int(Texture_unit::last_frame),
		                "texture_size", blur_size
		            ));

		_glow_shader.attach_shader(assets.load<Shader>("vert_shader:glow_filter"_aid))
		            .attach_shader(assets.load<Shader>("frag_shader:glow_filter"_aid))
		            .bind_all_attribute_locations(simple_vertex_layout)
		            .build()
		            .uniforms(make_uniform_map(
		                 "tex", int(Texture_unit::last_frame)
		             ));
		
		
		_blur_canvas_texture[0] = &_blur_canvas[0].get_attachment("color"_strid);
		_blur_canvas_texture[1] = &_blur_canvas[1].get_attachment("color"_strid);

		_motion_blur_canvas_texture = &_motion_blur_canvas.get_attachment("color"_strid);
	}

	void Renderer_effects::draw(graphic::Framebuffer& in) {
		auto depth1_cleanup = Disable_depthtest{};
		auto depth2_cleanup = Disable_depthwrite{};

		auto& current_frame = in.get_attachment("color"_strid);
		
		
		if(_graphics_ctx.settings().bloom) {
			_blur_canvas[0].bind_target();
			_blur_canvas[0].set_viewport();
			_blur_canvas[0].clear();

			_glow_shader.bind();
			graphic::draw_fullscreen_quad(current_frame, Texture_unit::last_frame);

			_blur_shader.bind();

			constexpr auto steps = 1;
			for(auto i : util::range(steps*2)) {
				auto src = i%2;
				auto dest = src>0?0:1;

				_blur_canvas[dest].bind_target();

				_blur_shader.set_uniform("horizontal", i%2==0);
				graphic::draw_fullscreen_quad(*_blur_canvas_texture[src], Texture_unit::temporary);
			}

			_blur_canvas_texture[0]->bind(int(Texture_unit::temporary));

			if(_motion_blur_intensity>0.f) {
				_motion_blur_shader.bind();
				_motion_blur_shader.set_uniform("dir", _motion_blur_dir*_motion_blur_intensity);

				_motion_blur_canvas.bind_target();
				_motion_blur_canvas.set_viewport();
				_motion_blur_canvas.clear();
				graphic::draw_fullscreen_quad(current_frame, Texture_unit::last_frame);
				in.bind_target();
				graphic::draw_fullscreen_quad(*_motion_blur_canvas_texture, Texture_unit::last_frame);
			}
		}
		
	}

}
}
}
