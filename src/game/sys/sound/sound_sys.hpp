/** System that manages sound effects ****************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/audio/audio_ctx.hpp>
#include <core/audio/sound.hpp>
#include <core/engine.hpp>
#include <core/utils/str_id.hpp>
#include <core/utils/messagebus.hpp>
#include <core/units.hpp>
#include <core/ecs/ecs.hpp>


namespace lux {
namespace graphic {
	struct Animation_event;
}
namespace sys {
namespace sound {

	struct Sound_mappings;


	class Sound_sys {
		public:
			Sound_sys(Engine&, ecs::Entity_manager& ecs);
			~Sound_sys();

			void update(Time dt);

		private:
			struct Sound_effect {
				audio::Sound_ptr sound;
				bool loop = false;
			};

			audio::Audio_ctx& _audio_ctx;
			asset::Asset_manager& _assets;
			ecs::Entity_manager& _ecs;
			asset::Ptr<Sound_mappings> _mappings;
			util::Mailbox_collection _mailbox;

			int _last_rev = 0;
			std::unordered_map<util::Str_id, Sound_effect> _event_sounds;


			void _reload();
			void _on_anim_event(const graphic::Animation_event& event);

	};

}
}
}
