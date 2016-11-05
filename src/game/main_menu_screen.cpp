#define GLM_SWIZZLE

#include "main_menu_screen.hpp"
#include "editor_screen.hpp"
#include "world_map_screen.hpp"


#include <core/renderer/graphics_ctx.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <core/gui/gui.hpp>
#include <core/gui/translations.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace lux {
	using namespace unit_literals;
	using namespace renderer;


	Main_menu_screen::Main_menu_screen(Engine& engine)
	    : Screen(engine),
	      _mailbox(engine.bus()),
	      _camera_ui(engine.graphics_ctx().viewport(), calculate_vscreen(engine, 1080))
	{

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "quit"_strid:
					_engine.screens().leave();
					break;
			}
		});

		_render_queue.shared_uniforms(renderer::make_uniform_map("vp", _camera_ui.vp()));
	}

	void Main_menu_screen::_on_enter(util::maybe<Screen&> prev) {
		_engine.input().enable_context("menu"_strid);
		_mailbox.enable();
	}

	void Main_menu_screen::_on_leave(util::maybe<Screen&> next) {
		_mailbox.disable();
	}

	void Main_menu_screen::_update(Time dt) {
		_mailbox.update_subscriptions();

		auto text = [&](auto& key) {
			return _engine.translator().translate("menu", key).c_str();
		};


		auto ctx = _engine.gui().ctx();
		if(nk_begin(ctx, "main_menu", _engine.gui().centered(200, 300), NK_WINDOW_BORDER)) {

			gui::begin_menu(ctx, _active_menu_entry);

			if(gui::menu_button(ctx, text("game"))) {
				_engine.screens().enter<World_map_screen>("jungle"); // TODO: selection screen for packs
			}

			if(gui::menu_button(ctx, text("editor"))) {
				_engine.screens().enter<Editor_screen>("jungle_01"); // TODO: selection screen for levels
			}

			if(gui::menu_button(ctx, text("options"), false)) {
				// TODO
			}

			if(gui::menu_button(ctx, text("quit"))) {
				_engine.screens().leave();
			}

			gui::end_menu(ctx);
		}
		nk_end(ctx);
	}


	void Main_menu_screen::_draw() {
		_render_queue.flush();
	}
}
