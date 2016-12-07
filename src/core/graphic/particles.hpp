/** 3D particle system *******************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "command_queue.hpp"
#include "shader.hpp"

#include "../units.hpp"


namespace lux {
	class Engine;

namespace graphic {

	using Particle_type_id = util::Str_id;

	struct Float_range {
		float min;
		float max;
	};

	struct Attractor_plane {
		glm::vec3 normal;
		float force = 0.f;
	};
	struct Attractor_point {
		glm::vec3 point;
		float force = 0.f;
	};

	struct Particle_type {
		Particle_type_id id;
		bool physics_simulation = false;
		float mass = 1.f;
		std::string texture;

		int animation_frames = 1;
		float fps = 1.f;
		float hue_change_in = 300;

		Float_range initial_alpha {1.f,1.f};
		Float_range final_alpha {1.f,1.f};
		Float_range initial_opacity {1.f,1.f};
		Float_range final_opacity {1.f,1.f};

		Float_range initial_size {0.2f,0.2f};
		Float_range final_size {0.2f,0.2f};
		Float_range rotation {0.f,0.f};
		Float_range lifetime {1.f,1.f};

		Float_range emision_rate {100.f, 100.f};
		int max_particle_count = 1000;

		Float_range spawn_x {-1.f,1.f};
		Float_range spawn_y {-1.f,1.f};
		Float_range spawn_z {-1.f,1.f};

		Float_range initial_pitch {0.f,0.f};
		Float_range initial_yaw {0.f,0.f};
		Float_range initial_roll {0.f,0.f};

		Float_range speed_pitch {0.f,0.f};
		Float_range speed_yaw {0.f,0.f};
		Float_range speed_roll {0.f,0.f};

		Float_range speed_pitch_global {0.f,0.f};
		Float_range speed_yaw_global {0.f,0.f};
		Float_range speed_roll_global {0.f,0.f};

		Float_range initial_speed {1.f,1.f};
		Float_range final_speed {1.f,1.f};

		Attractor_plane attractor_plane;
		Attractor_point attractor_point;

		float source_velocity_conservation = 0.f;
	};
	using Particle_type_ptr = asset::Ptr<Particle_type>;

	class Particle_emitter {
	public:
		Particle_emitter(const Particle_type& type)
				: _type(type) {}
		virtual ~Particle_emitter() = default;

		void position(glm::vec3 position) {
			_position = position;
		}
		void direction(glm::vec3 euler_angles) {
			_direction = glm::quat(euler_angles);
		}
		void scale(float s) {
			_scale = s;
		}

		void hue_change_out(Angle out) {
			_hue_out = out;
		}

		auto type()const noexcept {return _type.id;}
		virtual auto texture()const noexcept -> const Texture* = 0;
		virtual void update(Time dt) = 0;
		virtual bool draw(Command& cmd)const = 0;
		virtual void disable() {_active = false;}
		virtual bool dead()const noexcept {return !_active;}
		auto hue_change_in()const noexcept {return _type.hue_change_in;}
		auto hue_change_out()const noexcept {return _hue_out;}

	protected:
		const Particle_type& _type;

		glm::vec3 _position;
		glm::quat _direction;
		float _scale = 1.f;
		bool _active = true;
		Angle _hue_out = Angle::from_degrees(300);
	};
	using Particle_emitter_ptr = std::shared_ptr<Particle_emitter>;


	class Particle_renderer {
		public:
			Particle_renderer(asset::Asset_manager& assets);

			auto create_emiter(Particle_type_id) -> Particle_emitter_ptr;

			void update(Time dt);
			void draw(Command_queue&)const;

			void clear();

		private:
			std::unordered_map<Particle_type_id, Particle_type_ptr> _types;
			std::vector<Particle_emitter_ptr> _emitters;

			mutable Shader_program _simple_shader;
	};

}
}

#include <sf2/sf2.hpp>

namespace lux {
	namespace asset {

		template<>
		struct Loader<graphic::Particle_type> {
			static auto load(istream in) -> std::shared_ptr<graphic::Particle_type>;
			static void store(ostream out, const graphic::Particle_type& asset);
		};

	}
}
