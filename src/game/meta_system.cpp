#include "meta_system.hpp"

#include <core/graphic/graphics_ctx.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/uniform_map.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/framebuffer.hpp>
#include <core/graphic/texture_batch.hpp>
#include <core/graphic/primitives.hpp>


namespace lux {

	using namespace graphic;
	using namespace unit_literals;
	

	Meta_system::Meta_system(Engine& engine)
	    : entity_manager(engine),
	      scene_graph(entity_manager),
	      physics(engine, entity_manager),
	      controller(engine, entity_manager, physics),
	      camera(engine, entity_manager),
	      renderer(engine, entity_manager),
	      gameplay(engine, entity_manager, physics, camera, controller),
	      sound(engine, entity_manager),

	      _engine(engine) {
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

		renderer.lights().config().dir_light_color     = sun_light;
		renderer.lights().config().dir_light_direction = sun_dir;
		renderer.lights().config().ambient_light       = environment_brightness;

//		using namespace glm;
//		_skybox.tint(vec3(ambient_brightness) + sun_light);

//		_skybox.brightness(environment_brightness);
	}
	void Meta_system::light_config(Rgb sun_light, glm::vec3 sun_dir, float ambient_brightness,
	                               Rgba background_tint) {

		renderer.lights().config().dir_light_color     = sun_light;
		renderer.lights().config().dir_light_direction = sun_dir;

//		using namespace glm;
//		_skybox.tint(vec3(ambient_brightness) + sun_light);
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

		renderer.set_environment(level_meta_data.environment_id);

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

	void Meta_system::draw(util::maybe<const graphic::Camera&> cam_mb) {
		const auto& cam = cam_mb.get_or_other(camera.camera());


		renderer.effects().motion_blur(camera.motion_blur_dir(),
		                               camera.motion_blur());
		
		renderer.draw(cam);
	}

}
