#include "renderer_system.hpp"



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

		auto create_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine);
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_16F);
			framebuffer.add_depth_attachment("depth"_strid);
			framebuffer.build();

			return framebuffer;
		}
		auto wrap_no_depth(Framebuffer& fb) {
			auto& color = fb.get_attachment("color"_strid);
			
			auto framebuffer = Framebuffer(color.width(), color.height());
			framebuffer.add_color_attachment("color"_strid, 0, color);
			framebuffer.build();

			return framebuffer;
		}
		auto create_decals_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine) / 2.f;
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB);
			framebuffer.build();

			return framebuffer;
		}
	}
	
	
	Renderer_system::Renderer_system(Engine& engine, ecs::Entity_manager& entities)
	    : _assets(engine.assets()),
	      _graphics_ctx(engine.graphics_ctx()),
	      _global_uniforms(Global_uniforms{}),
	      _canvas{create_framebuffer(engine), create_framebuffer(engine)},
	      _canvas_no_depth{wrap_no_depth(_canvas[0]), wrap_no_depth(_canvas[1])},
	      _decals_canvas(create_decals_framebuffer(engine)),
	      
	      _forward_renderer(engine.bus(), entities, _assets),
	      _light_renderer(_graphics_ctx, entities, _assets),
	      _effect_renderer(engine),
	      _skybox(_assets) {

		_skybox.brightness(0.1f); // TODO
		
		_render_queue.shared_uniforms(std::make_shared<Uniform_map<4, 4*4*2*sizeof(float)>>());
		
		_post_shader.attach_shader(engine.assets().load<Shader>("vert_shader:post"_aid))
		            .attach_shader(engine.assets().load<Shader>("frag_shader:post"_aid))
		            .bind_all_attribute_locations(simple_vertex_layout)
		            .build()
		            .uniforms(make_uniform_map(
		                "tex", int(Texture_unit::last_frame),
		                "texture_glow", int(Texture_unit::temporary),
		                "gamma", _graphics_ctx.settings().gamma,
		                "texture_size", framebuffer_size(engine),
		                "exposure", 1.0f,
		                "bloom", (_graphics_ctx.settings().bloom ? 1.f : 0.f)
		            ));
		
		
		// cache the textures of each framebuffer
		_canvas_texture[0] = &_canvas[0].get_attachment("color"_strid);
		_canvas_texture[1] = &_canvas[1].get_attachment("color"_strid);
		
		_decals_canvas.get_attachment("color"_strid).bind(int(Texture_unit::decals));
		
		_global_uniforms.bind(int(Uniform_buffer_slot::globals));
	}
	
	void Renderer_system::update(Time dt) {
		_forward_renderer.update(dt);
		
		_time_acc += dt;
	}

	void Renderer_system::draw(const renderer::Camera& cam) {
		auto& canvas      = _canvas[_canvas_first_active ? 0: 1];
		auto& canvas_nd   = _canvas_no_depth[_canvas_first_active ? 0: 1];
		auto& frame       = *_canvas_texture[_canvas_first_active ? 0: 1];
		
		// set global uniform block
		auto globals = _set_global_uniforms(cam);
		
		_draw_decals();
		_decals_canvas.get_attachment("color"_strid).bind(int(Texture_unit::decals));

		_forward_renderer.draw();
		
		// draw lights and shadows
		_forward_renderer.flush_occluders(_light_renderer.occluder_command_queue());
		_light_renderer.pre_draw(globals); // TODO: _graphics_ctx.settings().light_quality
		
		// draw all objects
		_forward_renderer.flush_objects(_render_queue);
		_skybox.draw(_render_queue);

		canvas_nd.bind_target();
		_light_renderer.draw_light_volumns(_render_queue, canvas.get_attachment("depth"_strid));
		
		
		// flush queue to canvas
		canvas.bind_target();
		canvas.set_viewport();
		canvas.clear({0,0,0}, true);
		_render_queue.flush();
		
		
		// draw post effects and flush to back buffer
		_effect_renderer.draw(canvas);
		
		frame.bind(int(Texture_unit::last_frame));
		bind_default_framebuffer();
		_graphics_ctx.reset_viewport();
		_post_shader.bind().set_uniform("exposure", 1.0f)
		                   .set_uniform("contrast_boost", 0.f);//TODO: effects().motion_blur_intensity());
		graphic::draw_fullscreen_quad(frame, Texture_unit::last_frame);

		_canvas_first_active = !_canvas_first_active;


		debug_draw_framebuffer(_decals_canvas);
	}
	
	void Renderer_system::_draw_decals() {
		auto depth1_cleanup = Disable_depthtest{};
		auto depth2_cleanup = Disable_depthwrite{};
		auto blend_cleanup = Blend_add{};
		
		_decals_canvas.bind_target();
		_decals_canvas.set_viewport();
		_decals_canvas.clear({0,0,0}, true);
		_forward_renderer.draw_decals(_render_queue);
		_render_queue.flush();
	}
	
	Global_uniforms Renderer_system::_set_global_uniforms(const graphic::Camera& cam) {
		// compensates for screen-space technique limitations
		auto eye_pos = cam.eye_position();
		if(glm::length2(eye_pos-_last_sse_cam_position)>1.f) {
			_last_sse_cam_position = eye_pos;
			_last_sse_cam_position.z += 2.f + 5.f;
		}
		
		Global_uniforms globals;
		globals.view = cam.view();
		globals.proj = cam.proj();
		globals.vp = cam.vp();
		globals.vp_inv = glm::inverse(globals.vp);
		globals.eye = glm::vec4(cam.eye_position(), 1.f);
		
		auto sse_view = cam.view();
		sse_view[3] = glm::vec4(-_last_sse_cam_position, 1.f);
		globals.sse_vp = cam.proj() * sse_view;
		globals.sse_vp_inv = glm::inverse(globals.sse_vp);
		globals.time = _time_acc.value();
			
		_global_uniforms.set(globals);
		_render_queue.shared_uniforms()->emplace("vp", globals.vp);
		return globals;
	}
	
	void Renderer_system::set_environment(const std::string& id) {
		_skybox.texture(_assets.load<Texture>({"tex_cube"_strid, id}));
	}
	
	void Renderer_system::post_load() {
		_forward_renderer.post_load();
		_time_acc = Time{};
	}
	
}
}
}
