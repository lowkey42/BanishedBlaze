#define GLM_SWIZZLE

#include "editor_screen.hpp"

#include "game_screen.hpp"

#include "editor/editor_cmds.hpp"
#include "editor/level_settings.hpp"

#include "sys/physics/transform_comp.hpp"

#include <core/units.hpp>
#include <core/graphic/graphics_ctx.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/uniform_map.hpp>

#include <core/audio/music.hpp>
#include <core/audio/audio_ctx.hpp>

#include <core/gui/translations.hpp>
#include <core/gui/gui.hpp>

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iomanip>
#include <sstream>

namespace lux {
	using namespace unit_literals;
	using namespace graphic;
	using namespace editor;


	Editor_screen::Editor_screen(Engine& engine, const std::string& level_id)
	    : Screen(engine),
	      _mailbox(engine.bus()),
	      _input_manager(engine.input()),
	      _systems(engine),
	      _camera_menu(engine.graphics_ctx().viewport(),
	                   calculate_vscreen(engine, 1080)),
	      _camera_world(engine.graphics_ctx().viewport(), 80_deg, 5_m, 100_m),
	      _cmd_text(engine.assets().load<Font>("font:menu_font"_aid)),
	      _cmd_background(engine.assets().load<Texture>("tex:editor_cmd_background"_aid)),
	      _selection(engine, _systems.entity_manager, _camera_world, _commands),
	      _blueprints(engine, _commands, _selection, _systems.entity_manager, engine.assets(),
	                  engine.input(), _camera_world, _camera_menu, glm::vec2{_camera_menu.size().x/2.f, 0}),
	      _menu(engine, engine.assets(), _camera_menu),
	      _settings(engine.gui(), engine.translator(), _systems, _commands, _level_metadata),
	      _clipboard(util::nothing()),
	      _last_pointer_pos(util::nothing()),
	      _last_saved(_commands.get_last())
	{

		auto tooltip = [&](auto& key) {
			return _engine.translator().translate("editor_tooltip", key);
		};

		_menu.add_action("back"_strid, "tex:editor_icon_exit"_aid, tooltip("back"),
		                 [&]{_engine.screens().leave();});

		_menu.add_action("settings"_strid, "tex:editor_icon_settings"_aid, false, tooltip("settings"),
		                 [&](bool s){_settings.visible(s);});


		_menu.add_action("save"_strid, "tex:editor_icon_save"_aid, tooltip("save"),
		                 [&]{
			save_level(_engine, _systems.entity_manager, _level_metadata);
			_last_saved = _commands.get_last();
		}, [&]{return _last_saved!=_commands.get_last();} );

		_menu.add_action("load_prev"_strid, "tex:editor_icon_load_prev"_aid, tooltip("load_prev"),
		                 [&]{_load_next_level(-1);},
		                 [&]{return _load_next_level_allowed();} );

		_menu.add_action("load_next"_strid, "tex:editor_icon_load_next"_aid, tooltip("load_next"),
		                 [&]{_load_next_level(1);},
		                 [&]{return _load_next_level_allowed();} );


		_menu.add_action("cut"_strid, "tex:editor_icon_cut"_aid, tooltip("cut"),
		                 [&]{_clipboard = _selection.copy_content(); _commands.execute<Delete_cmd>(_selection);},
		                 [&]{return !!_selection.selection();} );

		_menu.add_action("copy"_strid, "tex:editor_icon_copy"_aid, tooltip("copy"),
		                 [&]{_clipboard = _selection.copy_content();},
		                 [&]{return !!_selection.selection();} );

		_menu.add_action("paste"_strid, "tex:editor_icon_paste"_aid, tooltip("paste"),
		                 [&]{
			_clipboard.process([&](auto& entity) {
				auto pos = this->_camera_world.eye_position();
				pos.z = 0.f;
				_commands.execute<Paste_cmd>(_systems.entity_manager, _selection, entity, pos);
			});
		}, [&]{return _clipboard.is_some();} );


		_menu.add_action("flip_h"_strid, "tex:editor_icon_flip_horiz"_aid, tooltip("flip_h"),
		                 [&]{_commands.execute<Flip_cmd>(_selection.selection(), false);},
		                 [&]{return !!_selection.selection();} );

		_menu.add_action("flip_v"_strid, "tex:editor_icon_flip_vert"_aid, tooltip("flip_v"),
		                 [&]{_commands.execute<Flip_cmd>(_selection.selection(), true);},
		                 [&]{return !!_selection.selection();} );


		_menu.add_action("undo"_strid, "tex:editor_icon_undo"_aid,  tooltip("undo"),
		                 [&]{_commands.undo();},
		                 [&]{return _commands.undo_available();});

		_menu.add_action("redo"_strid, "tex:editor_icon_redo"_aid,  tooltip("redo"),
		                 [&]{_commands.redo();},
		                 [&]{return _commands.redo_available();});


		_menu.add_action("zoom_out"_strid, "tex:editor_icon_zoom_out"_aid, tooltip("zoom_out"),
		                 [&]{_camera_world.move(glm::vec3{0,0,2.0} * 1_m);},
		                 [&]{return _camera_world.position().z.value()  <= 50.f;} );

		_menu.add_action("zoom_in"_strid, "tex:editor_icon_zoom_in"_aid, tooltip("zoom_in"),
		                 [&]{_camera_world.move(glm::vec3{0,0,-2.0} * 1_m);},
		                 [&]{return _camera_world.position().z.value()  >= 0.5f;} );


		_menu.add_action("light"_strid, "tex:editor_icon_toggle_light"_aid, true, tooltip("light"),
		                 [&](bool s) {
			if(s) {
				 _ambient_light_override = util::nothing();
			} else {
				 _ambient_light_override = 10.f;
			}
		});

		_menu.add_action("toggle_grid"_strid, "tex:editor_icon_snap_to_grid"_aid, true, tooltip("toggle_grid"),
		                 [&](bool s) {_selection.snap_to_grid(s);});

		_menu.add_action("unselect"_strid, "tex:editor_icon_unselect"_aid, tooltip("unselect"),
		                 [&]{_commands.execute<Selection_change_cmd>(_selection, ecs::Entity_facet{});},
		                 [&]{return !!_selection.selection();} );

		_menu.add_action("start"_strid, "tex:editor_icon_start"_aid, tooltip("start"), [&] {
			if(_last_saved!=_commands.get_last()) {
				save_level(_engine, _systems.entity_manager, _level_metadata);
			}
			_engine.screens().enter<Game_screen>(_level_metadata.id);
		});


		_mailbox.subscribe_to([&](input::Continuous_action& e) {
			switch(e.id) {
				case "scroll_l"_strid:
					_cam_speed.x += e.begin ? -1 : 1;
					break;
				case "scroll_r"_strid:
					_cam_speed.x += e.begin ? 1 : -1;
					break;
				case "scroll_d"_strid:
					_cam_speed.y += e.begin ? -1 : 1;
					break;
				case "scroll_u"_strid:
					_cam_speed.y += e.begin ? 1 : -1;
					break;
			}
		});


		_render_queue.shared_uniforms(graphic::make_uniform_map("vp", _camera_menu.vp()));

		_load_requested = level_id;
	}

	void Editor_screen::_load_next_level(int dir) {
		if(!_level_metadata.pack.empty()) {
			auto pack = get_level_pack(_engine,_level_metadata.pack);
			auto curr_index = pack->find_level(_level_metadata.id);
			if(curr_index.is_some()) {
				auto next_index = static_cast<std::size_t>(curr_index.get_or_throw()+dir) % pack->level_ids.size();

				DEBUG("Level "<<curr_index.get_or_throw()<<" => "<<next_index<<"  aka "<<pack->level_ids.at(next_index).aid);

				_load_requested = pack->level_ids.at(next_index).aid;
				_selection.select({});
				_commands.clear();
			} else {
				WARN("Level \""<<_level_metadata.id<<"\" not found in pack \""<<
				     _level_metadata.pack<<"\"");
			}
		} else {
			WARN("Level doesn't belong to a pack");
		}
	}
	bool Editor_screen::_load_next_level_allowed() {
		if(_level_metadata.pack.empty() || _load_requested.is_some())
			return false;

		auto pack = get_level_pack(_engine,_level_metadata.pack);
		auto curr_index = pack->find_level(_level_metadata.id);

		return curr_index.is_some();
	}

	void Editor_screen::_on_enter(util::maybe<Screen&>) {
		_engine.input().screen_to_world_coords([&](auto p) {
			return _camera_world.screen_to_world(p, glm::vec3(0,0,0)).xy();
		});
		_engine.input().world_space_events(false);
		_engine.input().enable_context("editor"_strid);
		_mailbox.enable();
		_menu.toggle_input(true);
	}

	void Editor_screen::_on_leave(util::maybe<Screen&>) {
		_menu.toggle_input(false);
		_mailbox.disable();
		_engine.input().world_space_events(true);
		_engine.input().screen_to_world_coords([](auto p) {
			return p;
		});
	}

	auto Editor_screen::_handle_pointer_cam(util::maybe<glm::vec2> mp1,
	                                        util::maybe<glm::vec2>) -> bool {

		_cam_mouse_active = _last_pointer_pos.is_some() && mp1.is_some();

		process(_last_pointer_pos,mp1) >> [&](auto last, auto curr) {
			auto wsrc = this->_camera_world.screen_to_world(last, glm::vec3(0,0,0));
			auto wtarget = this->_camera_world.screen_to_world(curr, glm::vec3(0,0,0));

			this->_camera_world.move((wsrc-wtarget)* 1_m);
		};

		return true;
	}

	void Editor_screen::_update(Time dt) {
		_mailbox.update_subscriptions();

		_systems.update(dt, Update::animations);
		_selection.update();
		_blueprints.update(dt);
		_menu.update(dt);


		auto pos = _selection.selection() ? remove_units(_selection.selection().get<sys::physics::Transform_comp>().get_or_throw().position())
		                                  : glm::vec3(_input_manager.last_pointer_world_position(), 0.0f);
		std::stringstream s;
		s << std::setw(15) <<std::left<< _level_metadata.id
		  << std::fixed << std::setprecision(2) << " "
		  <<std::setw(7)<<std::right<<pos.x<<" "
		  <<std::setw(7)<<std::right<<pos.y<<" "
		  <<std::setw(7)<<std::right<<pos.z;
		_cmd_text.set(s.str(), true);

		auto mp1 = _input_manager.pointer_screen_position(0);
		auto mp2 = _input_manager.pointer_screen_position(1);

		if(_cam_mouse_active) {
			_handle_pointer_cam(mp1,mp2);
		} else if(_selection.active()) {
			_selection.handle_pointer(mp1,mp2);

			if(_blueprints.is_in_delete_zone(_input_manager.last_pointer_screen_position()) && !_selection.active()) {
				_commands.undo();
			}
			_blueprints.handle_pointer(mp1,mp2);

		} else {
			bool unhandled = !_blueprints.handle_pointer(mp1,mp2) &&
			                 !_menu.handle_pointer(mp1,mp2) &&
			                 !_selection.handle_pointer(mp1,mp2) &&
			                 !_handle_pointer_cam(mp1,mp2);
			(void)unhandled;
		}

		_last_pointer_pos = mp1;


		auto curr_cam_speed = _cam_speed;

		if(mp1.is_some() && glm::length2(curr_cam_speed)<0.01f) {
			auto mps = _camera_menu.screen_to_world(mp1.get_or_throw(), 1.f);

			if(mps.x<=-_camera_menu.size().x*0.49f) {
				curr_cam_speed.x = -1.f;
			} else if(mps.x>=_camera_menu.size().x*0.49f) {
				curr_cam_speed.x = 1.f;
			}

			if(mps.y<=-_camera_menu.size().y*0.49f) {
				curr_cam_speed.y = 1.f;
			} else if(mps.y>=_camera_menu.size().y*0.49f) {
				curr_cam_speed.y = -1.f;
			}
		}

		_camera_world.move(glm::vec3(curr_cam_speed, 0.f) * dt.value() * 5_m);

		_settings.update_and_draw();
		if(!_settings.visible()) {
			_menu.force_toggle_state("settings"_strid, false);
		}

		if(_ambient_light_override.is_some()) {
			_systems.light_config(_level_metadata.environment_light_color,
		                          _level_metadata.environment_light_direction,
		                          _ambient_light_override.get_or_throw(),
		                          _level_metadata.background_tint);
		} else if(!_settings.visible()) {
			_systems.light_config(_level_metadata.environment_light_color,
		                          _level_metadata.environment_light_direction,
		                          _level_metadata.ambient_brightness,
		                          _level_metadata.background_tint);
		}

		if(_load_requested.is_some()) {
			if(_load_ready) {
				_level_metadata = _systems.load_level(_load_requested.get_or_throw(),
				                                      true);
				_load_ready = false;
				_load_requested = util::nothing();

			} else {
				auto load_text = _engine.translator().translate("editor", "loading").c_str();

				nk_begin_titled(_engine.gui().ctx(), "loading",
				                load_text,
				                _engine.gui().centered(200, 120),
				                NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE);
				nk_layout_row_dynamic(_engine.gui().ctx(),40, 1);
				nk_label(_engine.gui().ctx(), load_text, NK_TEXT_LEFT);
				nk_end(_engine.gui().ctx());
				_load_ready = true;
			}
		}
	}


	void Editor_screen::_draw() {

		_systems.draw(_camera_world);
		_render_queue.shared_uniforms()->emplace("vp", _camera_menu.vp());

		_blueprints.draw(_render_queue);

		_selection.draw(_render_queue, _camera_menu);

		_menu.draw(_render_queue);

		_batch.insert(*_cmd_background, glm::vec2(-_camera_menu.size().x/2.f+_cmd_background->width()/2.f,
		                                           _camera_menu.size().y/2.f-_cmd_background->height()/2.f));
		_batch.flush(_render_queue);

		_cmd_text.draw(_render_queue, glm::vec2(-_camera_menu.size().x/2.f+_cmd_text.size().x/2.f*0.25f + 10.f,
		                                         _camera_menu.size().y/2.f-_cmd_background->height()/1.5f), glm::vec4(1,1,1,1), 0.25f);

		_render_queue.flush();
	}
}
