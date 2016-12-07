#define GLM_SWIZZLE

#include "selection.hpp"

#include "editor_comp.hpp"
#include "editor_cmds.hpp"

#include "../sys/physics/transform_comp.hpp"
#include "../sys/renderer/renderer_forward.hpp"

#include <core/input/input_manager.hpp>
#include <core/graphic/primitives.hpp>
#include <core/graphic/command_queue.hpp>

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>


namespace lux {
namespace editor {

	using namespace unit_literals;
	using namespace graphic;
	using namespace lux::sys;
	using namespace sys::renderer;
	using namespace glm;

	namespace {

		bool is_inside(ecs::Entity_facet e, glm::vec2 p, Camera& cam, bool forgiving=false,
		               bool in_screenspace=false) {
			bool inside = false;

			auto terrain = e.get<Terrain_comp>();
			if(terrain.is_some()) {
				auto e_pos = remove_units(e.get<physics::Transform_comp>().get_or_throw().position());
				auto world_p = in_screenspace ? cam.screen_to_world(p, e_pos).xy() : p;

				auto icon_radius = glm::length(cam.screen_to_world(vec2{16.f, 16.f}, e_pos).xy() - cam.screen_to_world(vec2{0.f, 0.f}, e_pos).xy());

				auto& smart_texture = terrain.get_or_throw().smart_texture();

				if(forgiving && std::get<0>(smart_texture.get_point(e_pos.xy(),  world_p, icon_radius))!=Smart_texture::Point_location::none) {
					return true;
				}

				return smart_texture.is_inside(e_pos.xy(), world_p);
			}

			process(e.get<physics::Transform_comp>(), e.get<Editor_comp>())
			        >> [&](auto& transform, auto& editor) {
				auto center = remove_units(transform.position());
				auto half_bounds = remove_units(editor.bounds()).xy() * transform.scale() / 2.f;
				if(forgiving)
					half_bounds = glm::max(half_bounds, vec2{0.5f,0.5f}) + 0.5f;

				auto world_p = in_screenspace ? cam.screen_to_world(p, center).xy() : p;
				auto obb_p = rotate(world_p - center.xy(), -transform.rotation());

				inside = obb_p.x > -half_bounds.x && obb_p.x < half_bounds.x &&
				         obb_p.y > -half_bounds.y && obb_p.y < half_bounds.y;
			};

			return inside;
		}
	}

	Selection::Selection(Engine& engine, ecs::Entity_manager& entity_manager,
	                     graphic::Camera& world_cam, util::Command_manager& commands)
	    : _mailbox(engine.bus()), _world_cam(world_cam), _commands(commands),
	      _input_manager(engine.input()),
	      _ecs(entity_manager),
	      _editor_comps(entity_manager.list<Editor_comp>()),
	      _last_primary_pointer_pos(util::nothing()),
	      _last_secondary_pointer_pos(util::nothing()) {

		_icon_layer  = engine.assets().load<Texture>("tex:selection_icon_layer"_aid);
		_icon_move   = engine.assets().load<Texture>("tex:selection_icon_move"_aid);
		_icon_rotate = engine.assets().load<Texture>("tex:selection_icon_rotate"_aid);
		_icon_scale  = engine.assets().load<Texture>("tex:selection_icon_scale"_aid);


		_mailbox.subscribe_to([&](input::Continuous_action& e) {
			switch(e.id) {
				case "copy_drag"_strid:
					_copy_dragging = e.begin;
					break;
			}
		});
	}

	void Selection::draw(graphic::Command_queue& queue, graphic::Camera& cam) {
		if(_selected_entity) {
			auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();
			auto center = remove_units(transform.position());
			auto editor = _selected_entity.get<Editor_comp>();
			auto terrain = _selected_entity.get<Terrain_comp>();

			if(terrain.is_some()) {
				auto& points = terrain.get_or_throw().smart_texture().points();
				for(auto i=0u; i<points.size(); i++) {
					auto in = (i+1) % points.size();
					auto p  = points[i]+center.xy();
					auto pn = points[in]+center.xy();

					// world to screen
					p  = _world_cam.world_to_screen(vec3(p,  center.z));
					pn = _world_cam.world_to_screen(vec3(pn, center.z));

					// screen to menu
					p  = cam.screen_to_world(p).xy();
					pn = cam.screen_to_world(pn).xy();

					_batch.insert(*_icon_move, p, {32.f, 32.f});

					_batch.insert(*_icon_move, p+(pn-p)/2.f, {16.f, 16.f});

					draw_dashed_line(queue, p,
					                 pn,
					                 2.0f, Rgba{1.f,0.8f, 0.25f, 1.0f});
				}

				_batch.insert(*_icon_layer, cam.screen_to_world(_world_cam.world_to_screen(center)).xy(),
				              {32.f, 32.f});

				_batch.flush(queue);

			} else if(editor.is_some()) {
				auto bounds = remove_units(editor.get_or_throw().bounds()).xy() * transform.scale();
				bounds = glm::max(bounds, vec2{1,1});

				auto top_left     = vec2{-bounds.x/2.f,  bounds.y/2.f};
				auto bottom_right = top_left*-1.f;
				auto top_right    = glm::vec2{bottom_right.x, top_left.y};
				auto bottom_left  = glm::vec2{top_left.x, bottom_right.y};
				auto center_p     = glm::vec2{0,0};

				top_left     = rotate(top_left, transform.rotation())     + center.xy();
				bottom_right = rotate(bottom_right, transform.rotation()) + center.xy();
				top_right    = rotate(top_right, transform.rotation())    + center.xy();
				bottom_left  = rotate(bottom_left, transform.rotation())  + center.xy();
				center_p     = rotate(center_p, transform.rotation())     + center.xy();

				// world to screen
				top_left     = _world_cam.world_to_screen(vec3(top_left, center.z));
				bottom_right = _world_cam.world_to_screen(vec3(bottom_right, center.z));
				top_right    = _world_cam.world_to_screen(vec3(top_right, center.z));
				bottom_left  = _world_cam.world_to_screen(vec3(bottom_left, center.z));
				center_p     = _world_cam.world_to_screen(vec3(center_p, center.z));

				// screen to menu
				top_left     = cam.screen_to_world(top_left).xy();
				bottom_right = cam.screen_to_world(bottom_right).xy();
				top_right    = cam.screen_to_world(top_right).xy();
				bottom_left  = cam.screen_to_world(bottom_left).xy();
				center_p     = cam.screen_to_world(center_p).xy();

				draw_dashed_line(queue, top_left,     top_right,    2.0f, Rgba{1.f,0.8f, 0.25f, 1.0f});
				draw_dashed_line(queue, top_right,    bottom_right, 2.0f, Rgba{1.f,0.8f, 0.25f, 1.0f});
				draw_dashed_line(queue, bottom_right, bottom_left,  2.0f, Rgba{1.f,0.8f, 0.25f, 1.0f});
				draw_dashed_line(queue, bottom_left,  top_left,     2.0f, Rgba{1.f,0.8f, 0.25f, 1.0f});

				if(glm::length2(bottom_right-top_left)>32.f*32.f) {
					_batch.insert(*_icon_layer, top_left, {32.f, 32.f});
					_batch.insert(*_icon_rotate, bottom_right, {32.f, 32.f});
					_batch.insert(*_icon_scale, top_right, {32.f, 32.f});
					_batch.insert(*_icon_move, bottom_left, {32.f, 32.f});
				} else {
					_batch.insert(*_icon_move, center_p, {32.f, 32.f});
				}
				_batch.flush(queue);
			}
		}
	}
	void Selection::update() {
		_mailbox.update_subscriptions();
	}

	void Selection::_on_mouse_pressed(glm::vec2 mp) {
		_mouse_pressed_pos = mp;

		if(_selected_entity) {
			_curr_copy = _copy_dragging;
			_curr_copy_created = false;

			_current_action = Action_type::none;
			auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();

			_prev_entity_position = remove_units(transform.position());
			_prev_entity_rotation = transform.rotation();
			_prev_entity_scale = transform.scale();

			_curr_entity_position = _prev_entity_position;
			_curr_entity_rotation = _prev_entity_rotation;
			_curr_entity_scale    = _prev_entity_scale;
		}
	}
	void Selection::_on_mouse_released(glm::vec2 mp) {
		static constexpr auto epsilon = 0.0000001f;
		auto handled = false;

		if(_selected_entity && _current_action!=Action_type::none && _current_action!=Action_type::inactive) {
			if(_current_action==Action_type::mod_form && _selected_entity.has<Terrain_comp>()) {
				if(_snap_to_grid) {
					_curr_point_position = glm::round(_curr_point_position*2.f) / 2.f;
				}

				if(_point_created || glm::length2(_curr_point_position-_prev_point_position)>=epsilon) {
					handled = true;
					_commands.execute<Point_moved_cmd>(_selected_entity,
					                                   _current_shape_index,
					                                   _point_created,
					                                   _curr_point_position,
					                                   _prev_point_position);
				}

			} else if(_selected_entity.has<physics::Transform_comp>()) {
				auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();

				auto diff_pos = remove_units(transform.position()) - _prev_entity_position;
				auto diff_rot = transform.rotation() - _prev_entity_rotation;
				auto diff_scale = transform.scale() - _prev_entity_scale;
				auto copied = _curr_copy && _curr_copy_created;

				if(copied || glm::length2(diff_pos)>=epsilon
				        || std::abs(diff_rot)>=epsilon
				        || std::abs(diff_scale)>=epsilon) {
					handled = true;
					_commands.execute<Transform_cmd>(*this, _selected_entity,
					                                 _curr_copy && _curr_copy_created,
					                                 transform.position(),
					                                 transform.rotation(),
					                                 transform.scale(),
					                                 _prev_entity_position*1_m,
					                                 _prev_entity_rotation,
					                                 _prev_entity_scale);
				}
			}
		}

		_current_action = Action_type::inactive;


		// if click
		if(glm::length2(mp-_mouse_pressed_pos)<5.0f && !handled) {
			_change_selection(_mouse_pressed_pos);
		}
	}

	auto Selection::handle_pointer(util::maybe<glm::vec2> mp1,
	                               util::maybe<glm::vec2> mp2) -> bool {
		auto base_pos = _selected_entity ? remove_units(_selected_entity.get<physics::Transform_comp>().get_or_throw().position())
		                                 : glm::vec3(_world_cam.eye_position().xy(), 0.f);

		if(mp1.is_some()) {
			mp1 = _world_cam.screen_to_world(mp1.get_or_throw(), base_pos).xy();
		}
		if(mp2.is_some()) {
			mp2 = _world_cam.screen_to_world(mp2.get_or_throw(), base_pos).xy();
		}

		ON_EXIT {
			_last_primary_pointer_pos = mp1;
			_last_secondary_pointer_pos = mp2;
		};


		if(mp1.is_some() && !_last_primary_pointer_pos.is_some()) {
			_on_mouse_pressed(_input_manager.last_pointer_screen_position());

		} else if(!mp1.is_some() && _last_primary_pointer_pos.is_some()) {
			_on_mouse_released(_input_manager.last_pointer_screen_position());
		}

		if(mp1.is_nothing() || _last_primary_pointer_pos.is_nothing()
		        || !_selected_entity || _current_action==Action_type::inactive)
			return false;

		if(_current_action==Action_type::none && !is_inside(_selected_entity, _last_primary_pointer_pos.get_or_throw(), _world_cam, true))
			return false;


		if(mp2.is_some())
			return _handle_multitouch(mp1.get_or_throw(), mp2.get_or_throw());
		else
			return _handle_singletouch(_last_primary_pointer_pos.get_or_throw(), mp1.get_or_throw());
	}

	auto Selection::_handle_multitouch(glm::vec2 mp1_curr, glm::vec2 mp2_curr) -> bool {
		auto mp1_prev = _last_primary_pointer_pos.get_or_other(mp1_curr);
		auto mp2_prev = _last_secondary_pointer_pos.get_or_other(mp2_curr);

		_move(mp1_curr-mp1_prev);


		auto offset_curr = mp2_curr - mp1_curr;
		auto offset_prev = mp2_prev - mp1_prev;

		auto offset_curr_len = glm::length(offset_curr);
		auto offset_prev_len = glm::length(offset_prev);

		if(offset_prev_len>0.f && offset_curr_len>0.f) {
			_scale(mp1_curr, offset_curr_len / offset_prev_len);

			auto angle_curr = Angle{glm::atan(offset_curr.y, offset_curr.x)};
			auto angle_prev = Angle{glm::atan(offset_prev.y, offset_prev.x)};
			_rotate(mp1_curr, angle_prev - angle_curr);
		}

		return true;
	}
	auto Selection::_handle_singletouch(glm::vec2 prev, glm::vec2 curr) -> bool {
		auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();
		auto& editor    = _selected_entity.get<Editor_comp>().get_or_throw();
		auto terrain    = _selected_entity.get<Terrain_comp>();

		auto center      = remove_units(transform.position());

		auto obb_prev = rotate(prev - center.xy(), -transform.rotation());

		auto obb_curr = rotate(curr - center.xy(), -transform.rotation());

		auto icon_radius = glm::length(_world_cam.screen_to_world(vec2{16.f, 16.f}, center).xy() - _world_cam.screen_to_world(vec2{0.f, 0.f}, center).xy());
		auto icon_size2 = icon_radius*icon_radius;

		auto half_bounds = glm::max(remove_units(editor.bounds()).xy() * transform.scale(), vec2{1,1}) / 2.f;

		auto icon_layer_pos  = vec2{-half_bounds.x,  half_bounds.y};
		auto icon_rotate_pos = vec2{ half_bounds.x, -half_bounds.y};
		auto icon_scale_pos  = vec2{ half_bounds.x,  half_bounds.y};
		auto icon_move_pos   = vec2{-half_bounds.x, -half_bounds.y};

		if(_current_action==Action_type::none) {
			if(terrain.is_some()) {
				auto& smart_texture = terrain.get_or_throw().smart_texture();

				auto loc_type = Smart_texture::Point_location::none;
				auto index = 0;
				std::tie(loc_type,index) = smart_texture.get_point(center.xy(),
				                                                   prev, icon_radius);

				if(loc_type==Smart_texture::Point_location::on) {
					_current_action = Action_type::mod_form;
					_point_created = false;
					_current_shape_index = index;
					_prev_point_position = _curr_point_position = smart_texture.points()[_current_shape_index];

				} else if(loc_type==Smart_texture::Point_location::between) {
					_current_action = Action_type::mod_form;
					_point_created = true;
					_current_shape_index = _insert_point(index, curr);
					_prev_point_position = _curr_point_position = smart_texture.points()[_current_shape_index];

				} else if(glm::length2(prev-center.xy())<icon_size2) {
					_current_action = Action_type::layer;
				} else {
					_current_action = Action_type::move;
				}

			} else if(glm::length2(obb_prev-icon_move_pos)<icon_size2) {
				_current_action = Action_type::move;
			} else if(glm::length2(obb_prev-icon_layer_pos)<icon_size2) {
				_current_action = Action_type::layer;
			} else if(glm::length2(obb_prev-icon_rotate_pos)<icon_size2) {
				_current_action = Action_type::rotate;
			} else if(glm::length2(obb_prev-icon_scale_pos)<icon_size2) {
				_current_action = Action_type::scale;
			} else {
				_current_action = Action_type::move;
			}
		}

		switch(_current_action) {
			case Action_type::move:
				_move(curr - prev);
				break;

			case Action_type::scale:
				_scale(glm::length(obb_curr) / glm::length(obb_prev));
				break;

			case Action_type::rotate: {
				auto angle_curr = Angle{glm::atan(obb_curr.y, obb_curr.x)};
				auto angle_prev = Angle{glm::atan(obb_prev.y, obb_prev.x)};
				_rotate(angle_curr - angle_prev);
				break;
			}

			case Action_type::layer:
				_move_layer((curr.y - prev.y) * 1.f);
				break;

			case Action_type::mod_form:
				_move_point(curr - prev);
				break;

			case Action_type::inactive:
				FAIL("UNREACHABLE: Couldn't determine action_type (inactive)");
				break;
			case Action_type::none:
				FAIL("UNREACHABLE: Couldn't determine action_type (none)");
				break;
		}

		return true;
	}

	auto Selection::copy_content()const -> std::string {
		INVARIANT(_selected_entity, "No entity selected!");

		return _ecs.write_one(_selected_entity);
	}

	void Selection::_change_selection(glm::vec2 point, bool cycle) {
		ecs::Entity_facet entity;
		auto max_compv = std::make_pair(-1000.f, ecs::invalid_entity);
		auto limit_compv = std::make_pair(1000.f, ecs::invalid_entity);
		cycle = cycle && _selected_entity && is_inside(_selected_entity, point, _world_cam, false, true);
		if(cycle) {
			auto z = _selected_entity.get<physics::Transform_comp>().get_or_throw().position().z.value();
			limit_compv = std::make_pair(z, _selected_entity.handle());
		}

		for(auto& comp : _editor_comps) {
			if(is_inside(comp.owner(), point, _world_cam, false, true)) {
				auto z = comp.owner().get<physics::Transform_comp>().get_or_throw().position().z.value();
				auto compv = std::make_pair(z, comp.owner_handle());

				if(compv>max_compv && compv<limit_compv) {
					max_compv = compv;
					entity = comp.owner();
				}
			}
		}

		if(!entity && cycle) {
			_change_selection(point, false);
			return;
		}

		if(_selected_entity!=entity) {
			_commands.execute<Selection_change_cmd>(*this, entity);
		}
	}

	void Selection::_move(glm::vec2 offset) {
		if(!_selected_entity || glm::length2(offset)<=0.00001f)
			return;

		if(_curr_copy && !_curr_copy_created) {
			_selected_entity = _ecs.read_one(copy_content());
			_curr_copy_created = true;
		}

		_curr_entity_position += vec3(offset, 0.0f);
		_update_entity_transform();
	}
	void Selection::_move_layer(float offset) {
		_curr_entity_position.z += offset;
		_update_entity_transform();
	}
	void Selection::_rotate(Angle offset) {
		_curr_entity_rotation += offset;
		_update_entity_transform();
	}
	void Selection::_rotate(glm::vec2 pivot, Angle offset) {
		auto pos = _curr_entity_position;
		auto obj_pivot = pos.xy() - pivot;

		_curr_entity_position += vec3(rotate(obj_pivot, offset)-obj_pivot, 0.f);
		_curr_entity_rotation += offset;
		_update_entity_transform();
	}
	void Selection::_scale(float factor) {
		_curr_entity_scale *= factor;
		_update_entity_transform();
	}
	void Selection::_scale(glm::vec2 pivot, float factor) {
		auto pos = _curr_entity_position;
		auto obj_pivot = pos.xy() - pivot;
		_curr_entity_position += vec3((obj_pivot*factor)-obj_pivot, 0.f);

		_curr_entity_scale *= factor;
		_update_entity_transform();
	}
	auto Selection::_insert_point(int prev, glm::vec2 position) -> int {
		auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();
		auto& terrain = _selected_entity.get<renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		tex.insert_point(prev, position-remove_units(transform.position().xy()));
		return prev;
	}
	void Selection::_move_point(glm::vec2 offset) {
		auto& terrain = _selected_entity.get<renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		_curr_point_position += offset;

		util::maybe<std::size_t> new_point = util::nothing();

		if(_snap_to_grid) {
			new_point = tex.move_point(_current_shape_index, glm::round(_curr_point_position*2.f) / 2.f);

		} else {
			new_point = tex.move_point(_current_shape_index, _curr_point_position);
		}

		if(new_point.is_some()) {
			auto& smart_texture = _selected_entity.get<Terrain_comp>().get_or_throw().smart_texture();

			_commands.execute<Point_deleted_cmd>(_selected_entity,
			                                     _current_shape_index,
			                                     _prev_point_position);

			_current_shape_index = new_point.get_or_throw();
			_prev_point_position = _curr_point_position = smart_texture.points()[_current_shape_index];
			_point_created = false;
		}
	}

	void Selection::_update_entity_transform() {
		if(_selected_entity) {
			auto& transform = _selected_entity.get<physics::Transform_comp>().get_or_throw();

			if(_snap_to_grid) {
				auto rp = glm::round(_curr_entity_position*2.f)/2.f;
				rp.z = glm::round(_curr_entity_position.z*10.f)/10.f;
				transform.position(rp*1_m);
				transform.rotation(glm::round(_curr_entity_rotation/15_deg) * 15_deg);
				transform.scale(glm::ceil(_curr_entity_scale*4.f)/4.f);

			} else {
				transform.position(_curr_entity_position*1_m);
				transform.rotation(_curr_entity_rotation);
				transform.scale(_curr_entity_scale);
			}
		}
	}

}
}
