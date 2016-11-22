/** System that renders the representations of entites ***********************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "sprite_comp.hpp"
#include "terrain_comp.hpp"
#include "particle_comp.hpp"
#include "decal_comp.hpp"

#include "../../entity_events.hpp"

#include <core/renderer/camera.hpp>
#include <core/renderer/sprite_batch.hpp>
#include <core/renderer/particles.hpp>
#include <core/renderer/texture_batch.hpp>
#include <core/utils/messagebus.hpp>


namespace lux {
namespace sys {
namespace graphic {

	class Graphic_system {
		public:
			Graphic_system(util::Message_bus& bus,
			               ecs::Entity_manager& entity_manager,
			               asset::Asset_manager& asset_manager);

			void draw(const renderer::Camera& camera)const;
			void flush_occluders(renderer::Command_queue&);
			void flush_objects(renderer::Command_queue&);

			void draw_decals(renderer::Command_queue&,
			                 const renderer::Camera& camera)const;

			void update(Time dt);

			void post_load();

		private:
			void _on_state_change(const State_change&);

			renderer::Shader_program _occluder_shader;

			util::Mailbox_collection _mailbox;
			Sprite_comp::Pool& _sprites;
			Anim_sprite_comp::Pool& _anim_sprites;
			Terrain_comp::Pool& _terrains;
			Particle_comp::Pool& _particles;
			Decal_comp::Pool& _decals;

			renderer::Particle_renderer _particle_renderer;
			mutable renderer::Sprite_batch _sprite_batch;
			mutable renderer::Sprite_batch _sprite_batch_occluder;
			mutable renderer::Texture_batch _decal_batch;

			void _update_particles(Time dt);
	};

}
}
}
