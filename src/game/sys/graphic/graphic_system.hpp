/** System that renders the representations of entites ***********************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "../../entity_events.hpp"

#include <core/renderer/sprite_batch.hpp>
#include <core/renderer/camera.hpp>
#include <core/utils/messagebus.hpp>

#include "sprite_comp.hpp"


namespace lux {
namespace sys {
namespace graphic {

	class Graphic_system {
		public:
			Graphic_system(util::Message_bus& bus,
			               ecs::Entity_manager& entity_manager,
			               asset::Asset_manager& asset_manager);

			void draw(renderer::Command_queue&, const renderer::Camera& camera)const;
			void draw_shadowcaster(renderer::Sprite_batch&, const renderer::Camera& camera)const;
			void update(Time dt);

		private:
			void _on_state_change(const State_change&);


			util::Mailbox_collection _mailbox;
			Sprite_comp::Pool& _sprites;

			mutable renderer::Sprite_batch _sprite_batch;
	};

}
}
}
