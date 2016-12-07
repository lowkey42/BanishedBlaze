#include "editor_cmds.hpp"

#include "../sys/physics/transform_comp.hpp"
#include "../sys/renderer/renderer_forward.hpp"

#include <glm/gtx/string_cast.hpp>


namespace lux {
namespace editor {

	using namespace unit_literals;


	Delete_cmd::Delete_cmd(Selection& selection)
	    : _name("Entity deleted "+ecs::entity_name(selection.selection().handle())),
	      _selection(selection) {}

	void Delete_cmd::execute() {
		if(!_entity) {
			_entity = _selection.selection();
			INVARIANT(_entity, "No selected entity on execution of Delete_cmd");
			_saved_state = _entity.manager().write_one(_entity.handle());
		}

		_entity.manager().erase(_entity.handle());
		_selection.select({});
	}
	void Delete_cmd::undo() {
		INVARIANT(!_saved_state.empty(), "No stored entity in Delete_cmd");
		_entity = _entity.manager().read_one(_saved_state);
		_selection.select(_entity);
	}


	Paste_cmd::Paste_cmd(ecs::Entity_manager& ecs, Selection& selection, std::string data, glm::vec3 pos)
	    : _name("Entity pasted"),
	      _ecs(ecs), _selection(selection), _data(data), _pos(pos) {}

	void Paste_cmd::execute() {
		if(_entity) {
			_ecs.read_one(_data, _entity.handle());

		} else {
			_entity = _ecs.read_one(_data);
			auto trans_comp = _entity.get<sys::physics::Transform_comp>();
			trans_comp.process([&](auto& t){
				t.position(_pos * 1_m);
			});
		}

		_selection.select(_entity);
	}
	void Paste_cmd::undo() {
		INVARIANT(_entity, "No stored entity in Paste_cmd");
		_ecs.erase(_entity.handle());
		_selection.select({});
	}


	Flip_cmd::Flip_cmd(ecs::Entity_facet entity, bool vert)
	    : _name("Entity flipped"),
	      _entity(entity), _vert(vert) {}

	void Flip_cmd::execute() {
		auto& transform = _entity.get<sys::physics::Transform_comp>().get_or_throw();
		if(_vert) {
			transform.flip_vertical(!transform.flip_vertical());
		} else {
			transform.flip_horizontal(!transform.flip_horizontal());
		}
	}
	void Flip_cmd::undo() {
		execute();
	}


	Create_cmd::Create_cmd(ecs::Entity_manager& ecs, Selection& selection,
	                       std::string blueprint, glm::vec3 pos)
	    : _name("Entity created"),
	      _ecs(ecs), _selection(selection), _blueprint(blueprint), _pos(pos) {}

	void Create_cmd::execute() {
		if(_entity) {
			_ecs.read_one(_data, _entity.handle());

		} else {
			_entity = _ecs.emplace(_blueprint);
			auto trans_comp = _entity.get<sys::physics::Transform_comp>();
			trans_comp.process([&](auto& t){
				t.position(_pos * 1_m);
			});
		}

		_entity_prev_selected = _selection.selection();
		_selection.select(_entity);
	}
	void Create_cmd::undo() {
		INVARIANT(_entity, "No stored entity in Create_cmd");
		_data = _ecs.write_one(_entity.handle());
		_ecs.erase(_entity.handle());
		_selection.select(_entity_prev_selected);
	}


	Selection_change_cmd::Selection_change_cmd(Selection& mgr, ecs::Entity_facet e)
	    : _name("Selection changed "+ecs::entity_name(e.handle())),
	      _selection_mgr(mgr), _selection(e) {}

	void Selection_change_cmd::execute() {
		_prev_selection = _selection_mgr.selection();
		_selection_mgr.select(_selection);
	}
	void Selection_change_cmd::undo() {
		_selection_mgr.select(_prev_selection);
	}


	Transform_cmd::Transform_cmd(Selection& selection_mgr, ecs::Entity_facet e, bool copy,
	                             Position new_pos, Angle new_rot, float new_scale,
	                             Position prev_pos, Angle prev_rot, float prev_scale)
	    : _name("Entity Transformed "+ecs::entity_name(e)+" "+util::to_string(prev_scale)+" => "+util::to_string(new_scale)),
	      _selection_mgr(selection_mgr), _entity(e), _copied_entity(copy),
	      _new_position(new_pos), _new_rotation(new_rot), _new_scale(new_scale),
	      _prev_position(prev_pos), _prev_rotation(prev_rot), _prev_scale(prev_scale) {}

	void Transform_cmd::execute() {
		if(_copied_entity && !_saved_state.empty()) {
			_entity.manager().read_one(_saved_state, _entity.handle());
			_selection_mgr.select(_entity);
			return;
		}

		auto& transform = _entity.get<sys::physics::Transform_comp>().get_or_throw();

		transform.position(_new_position);
		transform.rotation(_new_rotation);
		transform.scale(_new_scale);
	}
	void Transform_cmd::undo() {
		if(_copied_entity) {
			_selection_mgr.select({});
			_saved_state = _entity.manager().write_one(_entity.handle());
			_entity.manager().erase(_entity.handle());
			return;
		}

		auto& transform = _entity.get<sys::physics::Transform_comp>().get_or_throw();

		transform.position(_prev_position);
		transform.rotation(_prev_rotation);
		transform.scale(_prev_scale);
	}


	Point_deleted_cmd::Point_deleted_cmd(ecs::Entity_facet e, int index, glm::vec2 prev_pos)
	    : _name("Smart texture modified. Point "+util::to_string(index)+" of "+ecs::entity_name(e)+" "+glm::to_string(prev_pos)+" deleted"),
	      _entity(e),
	      _index(index),
	      _prev_pos(prev_pos) {}

	void Point_deleted_cmd::execute() {
		if(_first_exec) {
			_first_exec = false;
			return;
		}

		auto& terrain = _entity.get<sys::renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		tex.erase_point(static_cast<std::size_t>(_index));
	}
	void Point_deleted_cmd::undo() {
		auto& terrain = _entity.get<sys::renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		tex.insert_point(static_cast<std::size_t>(_index), _prev_pos);
	}


	Point_moved_cmd::Point_moved_cmd(ecs::Entity_facet e, int index, bool new_point,
	                                 glm::vec2 new_pos, glm::vec2 prev_pos)
	    : _name("Smart texture modified. Point "+util::to_string(index)+" of "+ecs::entity_name(e)+" "+glm::to_string(prev_pos)+" => "+glm::to_string(new_pos)),
	      _entity(e),
	      _index(index), _new_point(new_point), _new_pos(new_pos), _prev_pos(prev_pos) {}

	void Point_moved_cmd::execute() {
		if(_first_exec) {
			// skip first because the point already exists on the current position
			_first_exec = false;
			return;
		}

		auto& terrain = _entity.get<sys::renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		if(_new_point) {
			tex.insert_point(static_cast<std::size_t>(_index), _new_pos);

		} else {
			tex.move_point(static_cast<std::size_t>(_index), _new_pos);
		}
	}
	void Point_moved_cmd::undo() {
		auto& terrain = _entity.get<sys::renderer::Terrain_comp>().get_or_throw();
		auto& tex = terrain.smart_texture();

		if(_new_point) {
			tex.erase_point(static_cast<std::size_t>(_index));
		} else {
			tex.move_point(static_cast<std::size_t>(_index), _prev_pos);
		}
	}

}
}
