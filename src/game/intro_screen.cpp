#include "intro_screen.hpp"

#include <core/units.hpp>
#include <core/graphic/graphics_ctx.hpp>

#include <core/audio/music.hpp>
#include <core/audio/audio_ctx.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace lux {
	using namespace unit_literals;
	using namespace graphic;

	Intro_screen::Intro_screen(Engine& game_engine)
	    : Screen(game_engine),
	      _mailbox(game_engine.bus()),
	      _camera(game_engine.graphics_ctx().viewport(), calculate_vscreen(game_engine, 512)),
	      _debug_Text(game_engine.assets().load<Font>("font:menu_font"_aid))
	{

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "back"_strid:
					_engine.exit();
					break;
			}
		});

		_render_queue.shared_uniforms(graphic::make_uniform_map("VP", _camera.vp()));
	}

	void Intro_screen::_on_enter(util::maybe<Screen&> prev) {
		//_engine.audio_ctx().play_music(_engine.assets().load<audio::Music>("music:intro"_aid));
	}

	void Intro_screen::_on_leave(util::maybe<Screen&> next) {

	}

	void Intro_screen::_update(Time) {
		_mailbox.update_subscriptions();

		auto p1 = _engine.input().pointer_screen_position(0).get_or_other({0,0});
		auto p2 = _engine.input().pointer_screen_position(1).get_or_other({0,0});
		_debug_Text.set("P1: "+util::to_string(p1.x)+" / "+util::to_string(p1.y)+"\n"+
						"P2: "+util::to_string(p2.x)+" / "+util::to_string(p2.y) );
	}


	void Intro_screen::_draw() {
		_debug_Text.draw(_render_queue,  glm::vec2(0,0), glm::vec4(1,1,1,1), 0.5f);

		_render_queue.flush();
	}
}
