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

#include <core/graphic/camera.hpp>
#include <core/graphic/sprite_batch.hpp>
#include <core/graphic/particles.hpp>
#include <core/graphic/texture_batch.hpp>
#include <core/graphic/shader.hpp>
#include <core/utils/messagebus.hpp>


namespace lux {
namespace sys {
namespace renderer {

	class Renderer_forward {
		public:
			Renderer_forward(util::Message_bus& bus,
			                  ecs::Entity_manager& entity_manager,
			                  asset::Asset_manager& asset_manager);

			void draw()const;
			void flush_occluders(graphic::Command_queue&);
			void flush_objects(graphic::Command_queue&);

			void draw_decals(graphic::Command_queue&)const;

			void update(Time dt);

			void post_load();

		private:
			void _on_state_change(const State_change&);
			
			graphic::Shader_program _occluder_shader;
			graphic::Shader_program _decal_shader;

			util::Mailbox_collection _mailbox;
			Sprite_comp::Pool& _sprites;
			Anim_sprite_comp::Pool& _anim_sprites;
			Terrain_comp::Pool& _terrains;
			Particle_comp::Pool& _particles;
			Decal_comp::Pool& _decals;

			graphic::Particle_renderer _particle_renderer;
			mutable graphic::Sprite_batch _sprite_batch;
			mutable graphic::Sprite_batch _sprite_batch_occluder;
			mutable graphic::Texture_batch _decal_batch;

			void _update_particles(Time dt);
	};

}
}
}
