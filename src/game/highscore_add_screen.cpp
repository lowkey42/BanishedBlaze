#include "highscore_add_screen.hpp"

#include "game_screen.hpp"
#include "level.hpp"

#include <core/units.hpp>
#include <core/renderer/graphics_ctx.hpp>

#include <core/audio/music.hpp>
#include <core/audio/audio_ctx.hpp>

#include <core/gui/gui.hpp>
#include <core/gui/translations.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iomanip>
#include <sstream>


namespace lux {
	using namespace unit_literals;
	using namespace renderer;

	Highscore_add_screen::Highscore_add_screen(Engine& engine, std::string level, Time time)
	    : Screen(engine),
	      _mailbox(engine.bus()),
	      _highscores(dynamic_cast<Game_engine&>(engine).highscore_manager()),
	      _camera(engine.graphics_ctx().viewport(), calculate_vscreen(engine, 1080)),
	      _debug_Text(engine.assets().load<Font>("font:menu_font"_aid)),
	      _highscore_list(_highscores.get_highscore(level)),
	      _level_id(std::move(level)),
	      _next_level_id(get_next_level(engine, _level_id)),
	      _time(time),
	      _player_name_str(_highscores.last_username())
	{
		_player_name.reset(_player_name_str);

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "quit"_strid:
					_engine.screens().leave(1);
					break;

				case "continue"_strid:
					_engine.screens().leave(2);
					break;

				case "next"_strid:
					if(_next_level_id.is_some()) {
						_engine.screens().leave(2);
						_engine.screens().enter<Game_screen>(_next_level_id.get_or_throw(), true);
					}
					break;
			}
		});

		_render_queue.shared_uniforms(renderer::make_uniform_map("VP", _camera.vp()));
	}

	void Highscore_add_screen::_on_enter(util::maybe<Screen&> prev) {
		_engine.input().enable_context("menu"_strid);
		_mailbox.enable();

		//_engine.audio_ctx().play_music(_engine.assets().load<audio::Music>("music:intro"_aid));
	}

	void Highscore_add_screen::_on_leave(util::maybe<Screen&> next) {
		_mailbox.disable();

		if(_highscore_list) {
			auto rank = _highscore_list->get_rank(_time);
			_player_name.get(_player_name_str);
			if(!_player_name_str.empty() && rank<10) {
				_highscores.push_highscore(_level_id, Highscore{_player_name_str, _time/1_s});
			}
		}
	}

	namespace {
		void print_score(nk_context* ctx, int rank, const std::string& name, float time) {
			std::stringstream ss;
			ss<<rank<<".";
			nk_label(ctx, ss.str().c_str(), NK_TEXT_RIGHT);
			nk_label(ctx, name.c_str(), NK_TEXT_CENTERED);

			ss.str(std::string());
			ss << std::setw(6) << std::right << std::fixed << std::setprecision(1) << time;
			nk_label(ctx, ss.str().c_str(), NK_TEXT_RIGHT);
		}
	}

	void Highscore_add_screen::_update(Time) {
		_mailbox.update_subscriptions();

		auto text = [&](auto& key) {
			return _engine.translator().translate("menu", key).c_str();
		};


		auto ctx = _engine.gui().ctx();
		if(nk_begin(ctx, "add_score", _engine.gui().centered(400, 600), NK_WINDOW_BORDER)) {

			if(_highscore_list || _highscore_list.try_load()) {
				auto rank = _highscore_list->get_rank(_time);

				_player_name.get(_player_name_str);
				auto player = "* "+_player_name_str+" *";

				nk_layout_row_dynamic(ctx, 30, 3);

				nk_label(ctx, text("rank"), NK_TEXT_RIGHT);
				nk_label(ctx, text("player"), NK_TEXT_CENTERED);
				nk_label(ctx, text("time"), NK_TEXT_RIGHT);

				int i=0;
				for(auto& score : _highscore_list->scores) {
					if(i++==rank) {
						print_score(ctx, rank+1, player, _time.value());
						i++;
					}

					print_score(ctx, i, score.name, score.time);

					if(i>=10) {
						break;
					}
				}
				if(i==rank) {
					print_score(ctx, rank+1, player, _time.value());
				}

				if(rank<10) {
					nk_layout_row_dynamic(ctx, 30, 2);
					nk_label(ctx, text("enter player name"), NK_TEXT_LEFT);
					_player_name.update_and_draw(ctx, NK_EDIT_FIELD);
				}

				nk_layout_row_dynamic(ctx, 30, 1);

				nk_layout_row_dynamic(ctx, 30, 3);
				if(nk_button_label(ctx, text("retry"))) {
					_engine.screens().leave(1);
				}
				if(nk_button_label(ctx, text("continue"))) {
					_engine.screens().leave(2);
				}
				if(_next_level_id.is_some() && nk_button_label(ctx, text("next level"))) {
					_engine.screens().leave(2);
					_engine.screens().enter<Game_screen>(_next_level_id.get_or_throw(), true);
				}

			} else {
				nk_label(ctx, text("loading highscores"), NK_TEXT_CENTERED);
			}

		}
		nk_end(ctx);
	}


	void Highscore_add_screen::_draw() {
		_render_queue.flush();
	}

}
