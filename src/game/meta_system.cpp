#include "meta_system.hpp"

#include <core/renderer/graphics_ctx.hpp>
#include <core/renderer/command_queue.hpp>
#include <core/renderer/uniform_map.hpp>
#include <core/renderer/texture.hpp>
#include <core/renderer/texture_batch.hpp>
#include <core/renderer/primitives.hpp>


namespace lux {

	using namespace renderer;
	using namespace unit_literals;

	namespace {

		constexpr auto global_uniforms = 4+sys::light::light_uniforms;
		constexpr auto global_uniforms_size = 4*(4*4)+sys::light::light_uniforms_size;
		constexpr auto global_uniforms_avg_size = (int)(global_uniforms_size/global_uniforms + 0.5f);

		using Global_uniform_map = renderer::Uniform_map<global_uniforms,
		                                                 global_uniforms_avg_size*sizeof(float)>;

		auto create_framebuffer(Engine& engine) {
			return Framebuffer{
				engine.graphics_ctx().win_width(),
				engine.graphics_ctx().win_height(),
				true, true};
		}
		auto create_blur_framebuffer(Engine& engine) {
			return Framebuffer{
				engine.graphics_ctx().win_width()/2,
				engine.graphics_ctx().win_height()/2,
				false, false};
		}
	}

	struct Meta_system::Post_renderer {
		public:
			Post_renderer(Engine& engine) :
			    graphics_ctx(engine.graphics_ctx()),
			    canvas{create_framebuffer(engine), create_framebuffer(engine)},
			    blur_canvas{create_blur_framebuffer(engine), create_blur_framebuffer(engine)}
			{
				render_queue.shared_uniforms(std::make_unique<Global_uniform_map>());

				auto blur_size = glm::vec2{engine.graphics_ctx().win_width()/2,
				                           engine.graphics_ctx().win_height()/2};

				post_shader.attach_shader(engine.assets().load<Shader>("vert_shader:post"_aid))
				            .attach_shader(engine.assets().load<Shader>("frag_shader:post"_aid))
				            .bind_all_attribute_locations(simple_vertex_layout)
				            .build()
				            .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::last_frame),
				                "texture_glow", int(Texture_unit::temporary),
				                "gamma", 2.2f,
				                "exposure", 1.0f,
				                "bloom", (graphics_ctx.bloom() ? 1.f : 0.f)
				            ));

				blur_shader.attach_shader(engine.assets().load<Shader>("vert_shader:blur"_aid))
				           .attach_shader(engine.assets().load<Shader>("frag_shader:blur"_aid))
				           .bind_all_attribute_locations(simple_vertex_layout)
				           .build()
				           .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::temporary),
				                "texture_size", blur_size
				            ));

				glow_shader.attach_shader(engine.assets().load<Shader>("vert_shader:glow_filter"_aid))
				           .attach_shader(engine.assets().load<Shader>("frag_shader:glow_filter"_aid))
				           .bind_all_attribute_locations(simple_vertex_layout)
				           .build()
				           .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::last_frame)
				            ));
			}

			Graphics_ctx& graphics_ctx;
			mutable renderer::Command_queue render_queue;
			renderer::Shader_program post_shader;
			renderer::Shader_program blur_shader;
			renderer::Shader_program glow_shader;

			renderer::Framebuffer canvas[2];
			renderer::Framebuffer blur_canvas[2];
			bool                  canvas_first_active = true;

			auto& active_canvas() {
				return canvas[canvas_first_active ? 0: 1];
			}

			void flush() {
				{
					auto fbo_cleanup = Framebuffer_binder{active_canvas()};
					active_canvas().clear();

					render_queue.flush();
				}

				if(graphics_ctx.bloom()) {
					auto fbo_cleanup = Framebuffer_binder{blur_canvas[0]};
					blur_canvas[0].clear();

					glow_shader.bind();
					renderer::draw_fullscreen_quad(active_canvas(), Texture_unit::last_frame);

					blur_shader.bind();

					constexpr auto steps = 16;
					for(auto i : util::range(steps*2)) {
						auto src = i%2;
						auto dest = src>0?0:1;

						auto fbo_cleanup = Framebuffer_binder{blur_canvas[dest]};

						blur_shader.set_uniform("horizontal", i%2==0);
						renderer::draw_fullscreen_quad(blur_canvas[src], Texture_unit::temporary);
					}

					blur_canvas[0].bind(int(Texture_unit::temporary));
				}

				post_shader.bind().set_uniform("exposure", 1.0f);
				renderer::draw_fullscreen_quad(active_canvas(), Texture_unit::last_frame);

				canvas_first_active = !canvas_first_active;
			}
	};

	Meta_system::Meta_system(Engine& engine)
	    : entity_manager(engine.assets()),
	      scene_graph(entity_manager),
	      physics(engine, entity_manager),
	      controller(engine, entity_manager, physics),
	      camera(engine, entity_manager),
	      lights(engine.bus(), entity_manager, engine.assets()),
	      renderer(engine.bus(), entity_manager, engine.assets()),
	      gameplay(engine, entity_manager, physics, camera),

	      _engine(engine),
	      _skybox(engine.assets(), "tex_cube:default_env"_aid),
	      _post_renderer(std::make_unique<Post_renderer>(engine)) {
	}

	Meta_system::~Meta_system() {
	}


	auto Meta_system::load_level(const std::string& id) -> Level_data {
		auto level_meta_data = ::lux::load_level(_engine, entity_manager, id);
		_skybox.texture(_engine.assets().load<Texture>({"tex_cube"_strid, level_meta_data.environment_id}));
		lights.config(level_meta_data.environment_light_color,
		              level_meta_data.environment_light_direction,
		              level_meta_data.ambient_brightness);

		return level_meta_data;
	}

	void Meta_system::update(Time dt, Update mask) {
		update(dt, static_cast<Update_mask>(mask));
	}

	void Meta_system::update(Time dt, Update_mask mask) {
		entity_manager.process_queued_actions();

		if(mask & Update::movements) {
			controller.update(dt);
			physics.update(dt);
			gameplay.update(dt);
			scene_graph.update(dt);
			camera.update(dt);
		}

		if(mask & Update::animations) {
			renderer.update(dt);
		}
	}

	void Meta_system::draw(util::maybe<const renderer::Camera&> cam_mb) {
		const renderer::Camera& cam = cam_mb.get_or_other(camera.camera());

		auto& queue = _post_renderer->render_queue;
		auto uniforms = queue.shared_uniforms();

		renderer.draw_shadowcaster(lights.shadowcaster_batch(), cam);
		lights.prepare_draw(queue, cam);

		uniforms->emplace("vp", cam.vp());
		uniforms->emplace("vp_inv", glm::inverse(cam.vp()));
		uniforms->emplace("eye", cam.eye_position());

		renderer.draw(queue, cam);

		_skybox.draw(queue);

		_post_renderer->flush();
	}

}
