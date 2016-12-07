#define GLM_SWIZZLE

#include "world_map_screen.hpp"
#include "level.hpp"
#include "loading_screen.hpp"

#include <core/gui/translations.hpp>
#include <core/gui/gui.hpp>

#include <core/graphic/graphics_ctx.hpp>

#include <core/audio/music.hpp>
#include <core/audio/sound.hpp>
#include <core/audio/audio_ctx.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace lux {
	using namespace unit_literals;
	using namespace graphic;


	World_map_screen::World_map_screen(Engine& engine, const std::string& level_pack_id)
	    : Screen(engine),
	      _mailbox(engine.bus()),
	      _ui_text(engine.assets().load<Font>("font:menu_font"_aid)),
	      _camera_ui(engine.graphics_ctx().viewport(), calculate_vscreen(engine, 1080)),
	      _level_pack(get_level_pack(engine,level_pack_id))
	{

		unlock_level(_engine, _level_pack->level_ids.front().aid);

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "quit"_strid:
					_engine.screens().leave();
					break;

				case "select_a"_strid:
					_enter_nth_level(0);
					break;
				case "select_b"_strid:
					_enter_nth_level(1);
					break;
				case "select_c"_strid:
					_enter_nth_level(2);
					break;
				case "select_d"_strid:
					_enter_nth_level(3);
					break;
				case "select_e"_strid:
					_enter_nth_level(4);
					break;
				case "select_f"_strid:
					_enter_nth_level(5);
					break;
				case "select_g"_strid:
					_enter_nth_level(6);
					break;
				case "select_h"_strid:
					_enter_nth_level(7);
					break;
				case "select_i"_strid:
					_enter_nth_level(8);
					break;
			}
		});

		_render_queue.shared_uniforms(graphic::make_uniform_map("vp", _camera_ui.vp()));
	}

	void World_map_screen::_enter_nth_level(std::size_t idx) {
		if(idx >= _level_pack->level_ids.size())
			return;

		auto id = _level_pack->level_ids.at(idx).aid;

		if(!is_level_locked(_engine,id))
			_engine.screens().enter<Game_screen>(id, true);
	}

	void World_map_screen::_on_enter(util::maybe<Screen&> prev) {
		_engine.input().enable_context("menu"_strid);
		_mailbox.enable();
	}

	void World_map_screen::_on_leave(util::maybe<Screen&> next) {
		_mailbox.disable();
	}

	void World_map_screen::_update(Time dt) {
		_mailbox.update_subscriptions();

		auto text = [&](auto& key) {
			return _engine.translator().translate("menu", key).c_str();
		};


		auto ctx = _engine.gui().ctx();
		if(nk_begin(ctx, "level_selection", _engine.gui().centered(300, 500), NK_WINDOW_BORDER)) {

			gui::begin_menu(ctx, _current_level);


			for(auto i=0u; i<_level_pack->level_ids.size(); i++) {
				auto id = _level_pack->level_ids.at(i).aid;

				if(is_level_locked(_engine,id))
					gui::menu_button(ctx, "[Locked]");

				else if(gui::menu_button(ctx, id.c_str())) {
					_enter_nth_level(i);
				}
			}

			if(gui::menu_button(ctx, text("quit"))) {
				_engine.screens().leave();
			}

			gui::end_menu(ctx);
		}
		nk_end(ctx);
	}


	void World_map_screen::_draw() {
		_render_queue.flush();
	}
}
