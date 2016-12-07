/**  Manages the lifetime and updates of other systems ***********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "level.hpp"

#include "sys/cam/camera_system.hpp"
#include "sys/controller/controller_system.hpp"
#include "sys/gameplay/gameplay_system.hpp"
#include "sys/renderer/renderer_system.hpp"
#include "sys/physics/transform_system.hpp"
#include "sys/physics/physics_system.hpp"
#include "sys/sound/sound_sys.hpp"

#include <core/engine.hpp>
#include <core/ecs/ecs.hpp>
#include <core/graphic/camera.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/skybox.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/shader.hpp>


namespace lux {

	enum class Update : unsigned int {
		none       = 0b0000,
		animations = 0b0001,
		movements  = 0b0010,
		gameplay   = 0b0100,
		input      = 0b1000
	};
	using Update_mask = unsigned int;

	constexpr Update_mask operator|(Update lhs, Update rhs)noexcept {
		return static_cast<Update_mask>(lhs) | static_cast<Update_mask>(rhs);
	}
	constexpr Update_mask operator|(Update_mask lhs, Update rhs)noexcept {
		return lhs | static_cast<Update_mask>(rhs);
	}
	constexpr bool operator&(Update_mask lhs, Update rhs)noexcept {
		return lhs & static_cast<Update_mask>(rhs);
	}

	constexpr auto update_all = Update::animations | Update::movements |
	                            Update::gameplay | Update::input;


	class Meta_system {
		public:
			Meta_system(Engine& engine);
			~Meta_system();

			auto load_level(const std::string& id, bool create=false) -> Level_info;

			void update(Time dt, Update_mask mask=update_all);
			void update(Time dt, Update update=Update::none);
			void draw(util::maybe<const graphic::Camera&> cam = util::nothing());

			void light_config(Rgb sun_light, glm::vec3 sun_dir, float ambient_brightness,
			                  Rgba background_tint, float environment_brightness);
			void light_config(Rgb sun_light, glm::vec3 sun_dir, float ambient_brightness,
			                  Rgba background_tint);

			ecs::Entity_manager entity_manager;
			sys::physics::Scene_graph scene_graph;
			sys::physics::Physics_system physics;
			sys::controller::Controller_system controller;
			sys::cam::Camera_system camera;
			sys::renderer::Renderer_system renderer;
			sys::gameplay::Gameplay_system gameplay;
			sys::sound::Sound_sys sound;

		private:
			Engine& _engine;
			std::string _current_level;

	};

}
