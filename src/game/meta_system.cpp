#include "meta_system.hpp"

#include <core/renderer/graphics_ctx.hpp>
#include <core/renderer/command_queue.hpp>
#include <core/renderer/uniform_map.hpp>
#include <core/renderer/texture.hpp>
#include <core/renderer/framebuffer.hpp>
#include <core/renderer/texture_batch.hpp>
#include <core/renderer/primitives.hpp>


namespace lux {

	using namespace renderer;
	using namespace unit_literals;

	namespace {

		using Global_uniform_map = renderer::Uniform_map<64,
		                                                 32*4*4*sizeof(float)>;

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
		auto create_effect_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine);
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_16F);
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
		auto create_blur_framebuffer(Engine& engine) {
			auto size = framebuffer_size(engine) / 2.f;
			auto framebuffer = Framebuffer(int(size.x), int(size.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_16F);
			framebuffer.build();

			return framebuffer;
		}
	}

	struct Meta_system::Post_renderer {
		public:
			Post_renderer(Engine& engine) :
			    graphics_ctx(engine.graphics_ctx()),
			    canvas{create_framebuffer(engine), create_framebuffer(engine)},
			    blur_canvas{create_blur_framebuffer(engine), create_blur_framebuffer(engine)},
			    motion_blur_canvas(create_effect_framebuffer(engine)),
			    decals_canvas(create_decals_framebuffer(engine))
			{
				render_queue.shared_uniforms(std::make_unique<Global_uniform_map>());

				auto blur_size = framebuffer_size(engine) / 2.f;

				post_shader.attach_shader(engine.assets().load<Shader>("vert_shader:post"_aid))
				            .attach_shader(engine.assets().load<Shader>("frag_shader:post"_aid))
				            .bind_all_attribute_locations(simple_vertex_layout)
				            .build()
				            .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::last_frame),
				                "texture_glow", int(Texture_unit::temporary),
				                "gamma", graphics_ctx.settings().gamma,
				                "texture_size", framebuffer_size(engine),
				                "exposure", 1.0f,
				                "bloom", (graphics_ctx.settings().bloom ? 1.f : 0.f)
				            ));

				blur_shader.attach_shader(engine.assets().load<Shader>("vert_shader:blur"_aid))
				           .attach_shader(engine.assets().load<Shader>("frag_shader:blur"_aid))
				           .bind_all_attribute_locations(simple_vertex_layout)
				           .build()
				           .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::temporary),
				                "texture_size", blur_size
				            ));

				motion_blur_shader
				           .attach_shader(engine.assets().load<Shader>("vert_shader:motion_blur"_aid))
				           .attach_shader(engine.assets().load<Shader>("frag_shader:motion_blur"_aid))
				           .bind_all_attribute_locations(simple_vertex_layout)
				           .build()
				           .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::last_frame),
				                "texture_size", blur_size
				            ));

				glow_shader.attach_shader(engine.assets().load<Shader>("vert_shader:glow_filter"_aid))
				           .attach_shader(engine.assets().load<Shader>("frag_shader:glow_filter"_aid))
				           .bind_all_attribute_locations(simple_vertex_layout)
				           .build()
				           .uniforms(make_uniform_map(
				                "texture", int(Texture_unit::last_frame)
				            ));


				// cache the textures of each framebuffer
				canvas_texture[0] = &canvas[0].get_attachment("color"_strid);
				canvas_texture[1] = &canvas[1].get_attachment("color"_strid);

				blur_canvas_texture[0] = &blur_canvas[0].get_attachment("color"_strid);
				blur_canvas_texture[1] = &blur_canvas[1].get_attachment("color"_strid);

				motion_blur_canvas_texture = &motion_blur_canvas.get_attachment("color"_strid);
				decals_canvas_texture = &decals_canvas.get_attachment("color"_strid);
			}

			Graphics_ctx& graphics_ctx;
			mutable renderer::Command_queue render_queue;
			renderer::Shader_program post_shader;
			renderer::Shader_program blur_shader;
			renderer::Shader_program motion_blur_shader;
			renderer::Shader_program glow_shader;

			renderer::Framebuffer    canvas[2];
			const renderer::Texture* canvas_texture[2];
			bool                     canvas_first_active = true;

			renderer::Framebuffer    blur_canvas[2];
			const renderer::Texture* blur_canvas_texture[2];
			renderer::Framebuffer    motion_blur_canvas;
			const renderer::Texture* motion_blur_canvas_texture;

			renderer::Framebuffer    decals_canvas;
			const renderer::Texture* decals_canvas_texture;

			glm::vec2 motion_blur_dir;
			float motion_blur_intensity = 0.f;

			auto& active_canvas() {
				return canvas[canvas_first_active ? 0: 1];
			}
			auto& active_canvas_texture() {
				return *canvas_texture[canvas_first_active ? 0: 1];
			}
			auto& inactive_canvas() {
				return canvas[canvas_first_active ? 1: 0];
			}

			void flush() {
				debug_draw_framebuffer(active_canvas());
				auto& canvas = active_canvas();
				canvas.bind_target();
				canvas.set_viewport();
				canvas.clear({0,0,0}, true);

				render_queue.flush();

				auto depth1_cleanup = Disable_depthtest{};
				auto depth2_cleanup = Disable_depthwrite{};

				auto& current_frame = active_canvas_texture();

				if(graphics_ctx.settings().bloom) {
					blur_canvas[0].bind_target();
					blur_canvas[0].set_viewport();
					blur_canvas[0].clear();

					glow_shader.bind();
					renderer::draw_fullscreen_quad(current_frame, Texture_unit::last_frame);

					blur_shader.bind();

					constexpr auto steps = 1;
					for(auto i : util::range(steps*2)) {
						auto src = i%2;
						auto dest = src>0?0:1;

						blur_canvas[dest].bind_target();

						blur_shader.set_uniform("horizontal", i%2==0);
						renderer::draw_fullscreen_quad(*blur_canvas_texture[src], Texture_unit::temporary);
					}

					blur_canvas_texture[0]->bind(int(Texture_unit::temporary));

					if(motion_blur_intensity>0.f) {
						motion_blur_shader.bind();
						motion_blur_shader.set_uniform("dir", motion_blur_dir*motion_blur_intensity);

						motion_blur_canvas.bind_target();
						motion_blur_canvas.set_viewport();
						motion_blur_canvas.clear();
						renderer::draw_fullscreen_quad(current_frame, Texture_unit::last_frame);
						active_canvas().bind_target();
						renderer::draw_fullscreen_quad(*motion_blur_canvas_texture, Texture_unit::last_frame);
						current_frame.bind(int(Texture_unit::last_frame));
					}
				}

				bind_default_framebuffer();
				graphics_ctx.reset_viewport();
				post_shader.bind().set_uniform("exposure", 1.0f)
				                  .set_uniform("contrast_boost", motion_blur_intensity);
				renderer::draw_fullscreen_quad(current_frame, Texture_unit::last_frame);

				canvas_first_active = !canvas_first_active;
			}
	};

	Meta_system::Meta_system(Engine& engine)
	    : entity_manager(engine),
	      scene_graph(entity_manager),
	      physics(engine, entity_manager),
	      controller(engine, entity_manager, physics),
	      camera(engine, entity_manager),
	      lights(engine.graphics_ctx(), entity_manager, engine.assets()),
	      renderer(engine.bus(), entity_manager, engine.assets()),
	      gameplay(engine, entity_manager, physics, camera, controller),
	      sound(engine, entity_manager),

	      _engine(engine),
	      _skybox(engine.assets()),
	      _post_renderer(std::make_unique<Post_renderer>(engine)) {
	}

	Meta_system::~Meta_system() {
		entity_manager.clear();
	}


	void Meta_system::light_config(Rgb sun_light, glm::vec3 sun_dir, float ambient_brightness,
	                               Rgba background_tint, float environment_brightness) {
		auto sun_dir_len = glm::length(sun_dir);
		if(sun_dir_len<0.0001f) {
			sun_dir = glm::normalize(glm::vec3{0.1f, -0.5f, 0.5f});
		} else {
			sun_dir = sun_dir / sun_dir_len;
		}

		lights.config().dir_light_color     = sun_light;
		lights.config().dir_light_direction = sun_dir;
		lights.config().ambient_light       = environment_brightness;

		using namespace glm;
		_skybox.tint(vec3(ambient_brightness) + sun_light);

		_skybox.brightness(environment_brightness);
	}
	void Meta_system::light_config(Rgb sun_light, glm::vec3 sun_dir, float ambient_brightness,
	                               Rgba background_tint) {

		lights.config().dir_light_color     = sun_light;
		lights.config().dir_light_direction = sun_dir;

		using namespace glm;
		_skybox.tint(vec3(ambient_brightness) + sun_light);
	}

	auto Meta_system::load_level(const std::string& id, bool create) -> Level_info {
		_current_level = id;
		auto level_meta_data = [&] {
			auto level = ::lux::load_level(_engine, entity_manager, id);
			if(level.is_nothing()) {
				INVARIANT(create, "Level doesn't exists: "<<id);

				entity_manager.clear();
				auto l = Level_info{};
				l.id = id;
				l.name = id;
				return l;
			}
			return level.get_or_throw();
		}();

		_skybox.texture(_engine.assets().load<Texture>({"tex_cube"_strid, level_meta_data.environment_id}));

		light_config(level_meta_data.environment_light_color,
		             level_meta_data.environment_light_direction,
	                 level_meta_data.ambient_brightness,
	                 level_meta_data.background_tint,
		             level_meta_data.environment_brightness);

		renderer.post_load();

		return level_meta_data;
	}

	void Meta_system::update(Time dt, Update mask) {
		update(dt, static_cast<Update_mask>(mask));
	}

	void Meta_system::update(Time dt, Update_mask mask) {
		entity_manager.process_queued_actions();

		if(mask & Update::input) {
			controller.update(dt);
			gameplay.update_pre_physic(dt);
		}

		if(mask & Update::movements) {
			physics.update(dt);
		}

		if(mask & Update::input) {
			gameplay.update_post_physic(dt);
			scene_graph.update(dt);
			sound.update(dt);
		}

		if(mask & Update::animations) {
			renderer.update(dt);
			camera.update(dt);
		}
	}

	void Meta_system::draw(util::maybe<const renderer::Camera&> cam_mb) {
		const renderer::Camera& cam = cam_mb.get_or_other(camera.camera());

		auto& queue = _post_renderer->render_queue;
		_post_renderer->decals_canvas.get_attachment("color"_strid).bind(int(Texture_unit::decals));

		auto& uniforms = *queue.shared_uniforms();
		const auto fast_lighting = _engine.graphics_ctx().settings().fast_lighting;
		uniforms.emplace("fast_lighting", fast_lighting);

		// draw objects to batches but don't flush to queue, yet
		renderer.draw(cam);

		// draw shadowmaps
		renderer.flush_occluders(lights.occluder_command_queue());
		auto light_quality = 1.0f;// fast_lighting ? 0.f : 1.f;
		lights.pre_draw(queue.shared_uniforms(), cam, light_quality);

		// draw decals
		{
			// reuses the shadow/light camera, that is further away from the scene,
			//  so this has to be drawn before the vp is reset
			auto depth1_cleanup = Disable_depthtest{};
			auto depth2_cleanup = Disable_depthwrite{};
			auto blend_cleanup = Blend_add{};
			_post_renderer->decals_canvas.bind_target();
			_post_renderer->decals_canvas.set_viewport();
			_post_renderer->decals_canvas.clear();
			renderer.draw_decals(queue, cam);
			queue.flush();
		}

		// draw all objects
		uniforms.emplace("view", cam.view());
		uniforms.emplace("proj", cam.proj());
		uniforms.emplace("vp", cam.vp());
		uniforms.emplace("vp_inv", glm::inverse(cam.vp()));
		uniforms.emplace("eye", cam.eye_position());

		renderer.flush_objects(queue);

		_skybox.draw(queue);

		lights.draw_light_volumns(queue, _post_renderer->inactive_canvas().get_attachment("depth"_strid));

		// draw post effects and flush
		_post_renderer->motion_blur_dir = camera.motion_blur_dir();
		_post_renderer->motion_blur_intensity = camera.motion_blur();
		_post_renderer->flush();
	}

}
