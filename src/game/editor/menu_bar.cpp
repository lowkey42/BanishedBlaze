#define GLM_SWIZZLE

#include "menu_bar.hpp"

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>

namespace lux {
namespace editor {

	using namespace unit_literals;

	Menu_bar::Action::Action(util::Str_id name, graphic::Texture_ptr icon, std::string tooltip,
	                         bool toggle_state, std::function<bool()> enabled,
	                         std::function<void()> callback, std::function<void(bool)> callback_toggle)
	    : name(std::move(name)),
	      icon(std::move(icon)),
	      tooltip(std::move(tooltip)),
	      toggle_state(std::move(toggle_state)),
	      enabled(std::move(enabled)),
	      callback(std::move(callback)),
	      callback_toggle(std::move(callback_toggle)) {
	}
	void Menu_bar::Action::call() {
		if((enabled && !enabled()) || !enabled_override)
			return;

		if(callback) {
			callback();

		} else if(callback_toggle) {
			toggle_state = !toggle_state;
			callback_toggle(toggle_state);
		}
	}

	Menu_bar::Menu_bar(Engine& engine, asset::Asset_manager& assets, graphic::Camera_2d& camera_ui)
	    : _mailbox(engine.bus()),
	      _assets(assets),
	      _camera_ui(camera_ui),
	      _input_manager(engine.input()),
	      _bg_left  (assets.load<graphic::Texture>("tex:editor_menu_left"_aid)),
	      _bg_center(assets.load<graphic::Texture>("tex:editor_menu_center"_aid)),
	      _bg_right (assets.load<graphic::Texture>("tex:editor_menu_right"_aid)),
	      _tooltip_text(engine.assets().load<graphic::Font>("font:menu_font"_aid)) {

		_mailbox.subscribe_to([&](input::Once_action& e) {
			util::maybe<Action&> action = util::nothing();

			if(e.id=="mouse_click"_strid) {
				action = _find_by_position(_camera_ui.screen_to_world(engine.input().last_pointer_screen_position(),1.f).xy());
			} else {
				action = _find_by_name(e.id);
			}

			action.process([](auto& a){a.call();});
		});
	}

	void Menu_bar::add_action(util::Str_id name, asset::AID icon, std::string tooltip,
	                          std::function<void()> callback,
	                          std::function<bool()> enabled) {

		_actions.emplace_back(std::move(name),
		                      _assets.load<graphic::Texture>(icon),
		                      std::move(tooltip),
		                      false,
		                      std::move(enabled),
		                      std::move(callback),
		                      std::function<void(bool)>{});
	}

	void Menu_bar::add_action(util::Str_id name, asset::AID icon, bool def_state, std::string tooltip,
	                                 std::function<void(bool)> callback,
	                                 std::function<bool()> enabled) {

		_actions.emplace_back(std::move(name),
		                      _assets.load<graphic::Texture>(icon),
		                      std::move(tooltip),
		                      def_state,
		                      std::move(enabled),
		                      std::function<void()>{},
		                      std::move(callback));
	}

	void Menu_bar::disable_action(util::Str_id name) {
		_find_by_name(name).process([](auto& a) {
			a.enabled_override = false;
		});
	}
	void Menu_bar::enable_action(util::Str_id name) {
		_find_by_name(name).process([](auto& a) {
			a.enabled_override = true;
		});
	}
	void Menu_bar::force_toggle_state(util::Str_id name, bool state) {
		_find_by_name(name).process([&](auto& a) {
			a.toggle_state = state;
		});
	}

	void Menu_bar::draw(graphic::Command_queue& queue) {
		auto y_offset    = _calc_y_offset();
		auto menu_length = _calc_menu_width();
		auto mouse_pos   = _to_local_offset(_last_mouse_screen_pos());
		auto mouse_down  = _input_manager.pointer_screen_position().is_some();

		_batch.insert(*_bg_left, {-menu_length/2.f + _bg_left->width()/2.f, y_offset});

		auto x_offset = -menu_length/2.f + _bg_left->width() + _bg_center->width()/2.f;
		int i = 0;
		for(auto& a : _actions) {
			auto pos = glm::vec2{x_offset, y_offset};
			_batch.insert(*_bg_center, pos);

			auto mouse_over = mouse_pos.x>=i && mouse_pos.x<i+1 && mouse_pos.y>=0.f && mouse_pos.y<=1.f;
			auto disabled = (a.enabled && !a.enabled()) || !a.enabled_override;
			auto size = glm::vec2{a.icon->width(), a.icon->height()/4.f};
			auto clip = glm::vec4{0,0,1.f,1/4.f};
			if(disabled) {
				clip.y += 3/4.f;
				clip.w += 3/4.f;
			} else if(a.toggle_state || (mouse_down && mouse_over)) {
				clip.y += 2/4.f;
				clip.w += 2/4.f;
			} else if(mouse_over) {
				clip.y += 1/4.f;
				clip.w += 1/4.f;
			}
			_batch.insert(*a.icon, pos, size, 0_deg, clip);

			x_offset += _bg_center->width();
			i++;
		}

		_batch.insert(*_bg_right, {menu_length/2.f - _bg_right->width()/2.f, y_offset});

		_batch.flush(queue);

		// draw tooltip
		if(_tooltip_text) {
			auto tt_pos = _tooltip_pos;
			tt_pos.y = -_camera_ui.size().y/2.f + _bg_center->height() + 20.f;
			_tooltip_text.draw(queue, tt_pos, glm::vec4{1,1,1,1}, 0.4f);
		}
	}

	void Menu_bar::update(Time dt) {
		_mailbox.update_subscriptions();

		auto mouse_pos = _last_mouse_screen_pos();

		if(glm::length2(_tooltip_pos-mouse_pos)<4.f) {
			if(_tooltip_delay_left>0_s) {
				_tooltip_delay_left -= dt;
			} else {
				auto active = _find_by_position(mouse_pos);
				_tooltip_text.set(active.is_some() ? active.get_or_throw().tooltip : "");
			}
		} else {
			_tooltip_pos = mouse_pos;
			_tooltip_delay_left = 0.2_s;
			_tooltip_text.set("");
		}

	}
	void Menu_bar::toggle_input(bool enable) {
		if(enable) {
			_mailbox.enable();
		} else {
			_mailbox.disable();
		}
	}

	auto Menu_bar::handle_pointer(util::maybe<glm::vec2>, util::maybe<glm::vec2>) -> bool {
		return _find_by_position(_last_mouse_screen_pos()).is_some();
	}

	auto Menu_bar::_find_by_position(glm::vec2 mouse_screen_pos) -> util::maybe<Action&> {
		auto mouse_pos   = _to_local_offset(mouse_screen_pos);
		auto x = static_cast<std::size_t>(std::floor(mouse_pos.x));

		if(x<0.f || x>=_actions.size() || mouse_pos.y<0.f || mouse_pos.y>1.f)
			return util::nothing();
		else
			return util::justPtr(&_actions.at(x));
	}
	auto Menu_bar::_find_by_name(util::Str_id name) -> util::maybe<Action&> {
		auto iter = std::find_if(_actions.begin(), _actions.end(), [&](auto& action) {
			return action.name == name;
		});

		return iter!=_actions.end() ? util::justPtr(&*iter) : util::nothing();
	}

	auto Menu_bar::_calc_y_offset()const -> float {
		return -_camera_ui.size().y/2.f + _bg_center->height()/2.f;
	}
	auto Menu_bar::_calc_menu_width()const -> float {
		auto bottons_length = _actions.size() * static_cast<float>(_bg_center->width());
		return bottons_length + _bg_left->width() + _bg_right->width();
	}

	auto Menu_bar::_to_local_offset(glm::vec2 p)const -> glm::vec2 {
		auto y_offset = _calc_y_offset();
		auto menu_length = _calc_menu_width();

		return {
			(p.x+menu_length/2.f-_bg_left->width()) / _bg_center->width(),
			(p.y-y_offset+_bg_center->height()/2.f) / _bg_center->height()
		};
	}

	auto Menu_bar::_last_mouse_screen_pos()const -> glm::vec2 {
		return _camera_ui.screen_to_world(_input_manager.last_pointer_screen_position(), 1.f).xy();
	}

}
}
