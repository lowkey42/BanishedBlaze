#define GLM_SWIZZLE

#include "game_screen.hpp"
#include "level.hpp"
#include "loading_screen.hpp"
#include "highscore_add_screen.hpp"

#include <core/graphic/graphics_ctx.hpp>

#include <core/audio/music.hpp>
#include <core/audio/sound.hpp>
#include <core/audio/audio_ctx.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iomanip>
#include <sstream>


namespace lux {
	using namespace unit_literals;
	using namespace graphic;

	namespace {
		constexpr auto fadeout_delay = 2_s;
		const auto fadeout_sun = Rgb{1.8, 1.75, 0.78} *4.f;
	}

	Game_screen::Game_screen(Engine& engine, const std::string& level_id, bool add_to_highscore)
	    : Screen(engine),
	      _mailbox(engine.bus()),
	      _systems(engine),
	      _add_to_highscore(add_to_highscore),
	      _ui_text(engine.assets().load<Font>("font:menu_font"_aid)),
	      _hud_background(engine.assets().load<Texture>("tex:hud_background"_aid)),
	      _hud_timer_background(engine.assets().load<Texture>("tex:hud_timer_background"_aid)),
	      _hud_light_icon(engine.assets().load<Texture>("tex:hud_light_icon"_aid)),
	      _hud_dash_icon(engine.assets().load<Texture>("tex:hud_dash_icon"_aid)),
	      _hud_foreground(engine.assets().load<Texture>("tex:hud_foreground"_aid)),
	      _players(_systems.entity_manager.list<sys::gameplay::Player_tag_comp>()),

	      _camera_ui(engine.graphics_ctx().viewport(),
	                 calculate_vscreen(engine, 1080)),
	      _current_level(level_id)
	{

		_orb_shader.attach_shader(engine.assets().load<Shader>("vert_shader:orb"_aid))
		           .attach_shader(engine.assets().load<Shader>("frag_shader:orb"_aid))
		           .bind_all_attribute_locations(simple_vertex_layout)
		           .build()
		           .uniforms(make_uniform_map(
		               "texture", int(Texture_unit::color)
		           ));

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "pause"_strid:
					_engine.screens().leave();
					break;
			}
		});

		_mailbox.subscribe_to([&](sys::gameplay::Level_finished& e){
			DEBUG("reached the end of the level");
			_fadeout = true;
			_engine.audio_ctx().stop_music(fadeout_delay*0.75f);
		});

		_render_queue.shared_uniforms(graphic::make_uniform_map("vp", _camera_ui.vp()));

		auto metadata = _systems.load_level(level_id);
		_music_aid = metadata.music_id;
	}
	Game_screen::~Game_screen()noexcept {
		_engine.audio_ctx().stop_sounds();
	}

	void Game_screen::_on_enter(util::maybe<Screen&> prev) {
		_engine.input().enable_context("game"_strid);
		_mailbox.enable();
		_engine.input().screen_to_world_coords([&](auto p) {
			return _systems.camera.screen_to_world(p).xy();
		});
		_engine.audio_ctx().play_music(_engine.assets().load<audio::Music>(asset::AID{"music"_strid, _music_aid}), 1_s);
		_engine.audio_ctx().resume_sounds();
	}

	void Game_screen::_on_leave(util::maybe<Screen&> next) {
		_engine.input().screen_to_world_coords([](auto p) {
			return p;
		});
		_mailbox.disable();
		_engine.audio_ctx().stop_music();
		_engine.audio_ctx().pause_sounds();

	}

	void Game_screen::_update(Time dt) {
		_mailbox.update_subscriptions();

		if(_reset_gameplay) {
			_reset_gameplay = false;

			// reset the game to be ready for continue
			_systems.renderer.lights().config().dir_light_color = _org_sun_light.get_or_throw();
			_fadeout = false;
			_fadeout_fadetimer = 0_s;
			_systems.gameplay.reset();
		}


		if(_selection_movement>0.f) {
			_selection_movement -= 4.f*dt.value();

			if(_selection_movement<0.0001f)
				_selection_movement = 0.f;

		} else if(_selection_movement<0.f) {
			_selection_movement += 4.f*dt.value();

			if(_selection_movement>-0.0001f)
				_selection_movement = 0.f;
		}

		if(_fadeout) {
			_systems.update(dt, Update::animations | Update::movements | Update::gameplay);
		} else {
			_time_acc+=dt;
			_systems.update(dt, update_all);
		}

		if(_systems.gameplay.game_time()>0.0_s) {
			std::stringstream s;
			s << std::setw(6) <<std::right<< std::fixed << std::setprecision(1)
			  << _systems.gameplay.game_time().value();
			_ui_text.set(s.str(), true);
		} else
			_ui_text.set("");

		if(_fadeout) {
			_fadeout_fadetimer+=dt;

			if(_org_sun_light.is_nothing()) {
				_org_sun_light = _systems.renderer.lights().config().dir_light_color;
			}
			_systems.renderer.lights().config().dir_light_color = glm::mix(_org_sun_light.get_or_throw(),
			                                                    fadeout_sun,
			                                                    _fadeout_fadetimer/fadeout_delay);

			if(_fadeout_fadetimer>=fadeout_delay) {
				unlock_next_levels(_engine, _current_level);
				if(_add_to_highscore) {
					_reset_gameplay = true;
					_engine.screens().enter<Highscore_add_screen>(_current_level, _systems.gameplay.game_time());

				} else {
					_engine.screens().leave();
				}
			}
		}
	}


	auto Game_screen::_draw_orb(glm::vec2 pos, float scale, ecs::Entity_facet e) -> graphic::Command {
		auto c = e.get<sys::gameplay::Enlightened_comp>().process(Rgb{1,1,1},
		                                                          [&](auto& l) {
			if(l.enabled()) {
				scale *= (l.air_time_percent_left()*0.9f + 0.1f);
			}

			return to_rgb(l.color());
		});


		auto cmd = create_command()
		        .order_dependent()
		        .shader(_orb_shader)
		        .object(graphic::quat_obj())
		        .texture(graphic::Texture_unit::color, *_hud_light_icon);

		cmd.uniforms().emplace("position", pos)
		        .emplace("scale", scale*_hud_light_icon->height()*0.25f)
		        .emplace("pos", pos)
		        .emplace("color", c)
		        .emplace("time", _time_acc.value());

		return cmd;
	}

	namespace {
		const auto orb_center = glm::vec2{235, 310} / 2.f;
		const auto orb_offset = (310-108) / 2.f;

		constexpr auto eular_mascheroni = 0.57721566490153286060f;

		auto nth_harmonic(float n) {
			if(n<0.0001f)
				return 0.f;

			return std::log(n) + eular_mascheroni
			        + 1/(2*n) - 1/(12*n*n);
		}

		auto orb_pos(float offset) {
			auto abs_offset = std::abs(offset);
			auto a = std::ceil(abs_offset) - abs_offset;
			auto h = nth_harmonic(std::floor(abs_offset))*a +
			         nth_harmonic(std::ceil(abs_offset))*(1.f-a);
			auto angle = 45_deg * h;
			return orb_center + rotate(glm::vec2{0,-orb_offset}, angle * glm::sign(offset));

		}
	}
	void Game_screen::_draw_orbs(sys::gameplay::Player_tag_comp::Pool::iterator selected,
	                             bool left_side, int count, glm::vec2 hud_pos) {

		auto left_count = std::ceil((count-1) / 2.f);
		auto right_count = count-1-left_count;

		auto orb_count = left_side ? left_count : right_count;

		// FIXME
		auto iter = selected;
		for(auto i=0; i<orb_count; i++) {
			if(left_side) {
				if(iter==_players.begin()) {
					iter = _players.end();
				}
				iter--;
			} else {
				iter++;
				if(iter==_players.end()) {
					iter = _players.begin();
				}
			}

			auto n = (i+1) * (left_side ? -1.f : 1.f) + _selection_movement;
			_render_queue.push_back(_draw_orb(hud_pos+orb_pos(n), 1.f/(1+std::abs(n)),
			                                  iter->owner()));
		}

	}

	void Game_screen::_draw() {
		_systems.draw();

		auto hud_pos = -_camera_ui.size()/2.f + glm::vec2(10,10);
		auto bg_pos = hud_pos + glm::vec2{_hud_background->width(), _hud_background->height()}*0.5f/2.f;

		if(_ui_text) {
			_render_queue.push_back(draw_texture(*_hud_timer_background, bg_pos, 0.5f));
			auto timer_pos = hud_pos + glm::vec2{500, 200}*0.5f;
			_ui_text.draw(_render_queue, timer_pos + glm::vec2(_ui_text.size().x/2.f*.75f, 0.f), glm::vec4(1,1,1,1), 0.75f);
		}

		_render_queue.push_back(draw_texture(*_hud_background, bg_pos, 0.5f));

		// draw light icons
		if(_systems.controller.get_controlled()) {
			auto player_count = static_cast<int>(_players.size());
			auto curr_player = _systems.controller.get_controlled();

			auto curr_player_iter = _players.begin();
			auto curr_player_idx = 0;
			for(; curr_player_iter!=_players.end(); curr_player_iter++, curr_player_idx++) {
				if(curr_player_iter->owner_handle() == curr_player) {
					break;
				}
			}

			auto selection_movement = curr_player_idx - _last_selected_idx;
			if(selection_movement > player_count/2)
				selection_movement = -(player_count - selection_movement);
			else if(selection_movement  < -player_count/2)
				selection_movement = player_count+selection_movement;

			_last_selected_idx = curr_player_idx;

			_selection_movement+=selection_movement;


			_render_queue.push_back(_draw_orb(hud_pos+orb_pos(_selection_movement), 1.f/(1.f+std::abs(_selection_movement)),
			                                  _systems.controller.get_controlled()));

			_draw_orbs(curr_player_iter, true, player_count, hud_pos);

			_draw_orbs(curr_player_iter, false, player_count, hud_pos);
		}

		// draw dash icons
		if(_systems.controller.get_controlled()) {
			auto light = _systems.controller.get_controlled().get<sys::gameplay::Enlightened_comp>();
			light.process([&](auto& l) {
				if(l.air_transforms_left()<1)
					return;

				for(auto i : util::range(l.air_transforms_left())) {
					auto p = hud_pos + glm::vec2{235, 280}/2.f;
					if(i%2!=0 || i+1<l.air_transforms_left()) {
						p.x += _hud_dash_icon->width()*0.25f * ((i%2==0) ? -1 : 1);
					}

					p.y += _hud_dash_icon->height()*0.5f * std::floor(i/2.f);
					_render_queue.push_back(draw_texture(*_hud_dash_icon, p, 0.6f));
				}
			});
		}

		_render_queue.push_back(draw_texture(*_hud_foreground, bg_pos, 0.5f));

		_render_queue.flush();
	}
}
