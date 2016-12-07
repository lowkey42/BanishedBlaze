/** Description of a view into the scene *************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../units.hpp"

namespace lux {
	class Engine;

namespace graphic {

	extern glm::vec2 calculate_vscreen(const Engine& engine, int target_height);


	class Camera {
		public:
			Camera(const glm::vec4& viewport, glm::mat4 proj);

			auto& view() const noexcept {
				if(_dirty){
					_view = _calc_view();
					_inv_view = glm::inverse(_view);
					_vp = _projection * _view;
					_dirty = false;
				}

				return _view;
			}
			auto& proj() const noexcept {
				return _projection;
			}
			auto& vp() const noexcept {
				view();
				return _vp;
			}
			virtual auto eye_position() const noexcept -> glm::vec3 {
				return glm::vec3(_inv_view * glm::vec4(0,0,0,1));
			}

			auto screen_to_world(glm::vec2 screen_pos, glm::vec3 expected_pos) const noexcept -> glm::vec3;
			auto screen_to_world(glm::vec2 screen_pos, float depth=0.99f) const noexcept -> glm::vec3;
			auto world_to_screen(glm::vec3 world_pos) const noexcept -> glm::vec2;

			auto viewport()const -> auto& {return _viewport;}

		protected:
			virtual auto _calc_view()const noexcept -> glm::mat4 =0;
			void _set_dirty()noexcept {_dirty = true;}

		private:
			const glm::vec4   _viewport;
			const glm::mat4   _projection;

			mutable glm::mat4 _vp;
			mutable glm::mat4 _view;
			mutable glm::mat4 _inv_view;
			mutable bool      _dirty = true;
	};

	class Camera_2d : public Camera {
		public:
			Camera_2d(const glm::vec4& viewport,
			          glm::vec2 size,
			          float world_scale=1.f,
			          glm::vec2 position=glm::vec2(0.0f),
			          float zoom=1.0f) noexcept;

			void position(glm::vec2 pos)    noexcept {_pos = pos;   _set_dirty();}
			void move    (glm::vec2 offset) noexcept {_pos+=offset; _set_dirty();}
			void zoom    (float z)          noexcept {_zoom=z;      _set_dirty();}

			auto zoom       () const noexcept {return _zoom;}
			auto position   () const noexcept {return _pos;}
			auto world_scale() const noexcept {return _world_scale;}
			auto size       () const noexcept {return _size;}

			auto area()const noexcept -> glm::vec4;

		private:
			auto _calc_view()const noexcept -> glm::mat4 override;

			const glm::vec2 _size;
			const float     _world_scale;
			glm::vec2       _pos;
			float           _zoom;
	};

	class Camera_3d : public Camera {
		public:
			Camera_3d(const glm::vec4& viewport,
			          Angle fov,
			          glm::vec3 pos, glm::vec3 look_at,
			          float near=.2f, float far=1000.f) noexcept;

			void position(glm::vec3 pos) noexcept {_pos = pos;      _set_dirty();}
			void rotation(glm::quat rot) noexcept;

			auto position() const noexcept {return _pos;}
			auto rotation() const noexcept {return _rotation;}

			void rotate_local (float roll, float pitch, float yaw) noexcept;
			void rotate_global(float roll, float pitch, float yaw) noexcept;
			void look_at      (glm::vec3 target)   noexcept;

			auto eye_position() const noexcept -> glm::vec3 override {
				return _pos;
			}

		protected:
			auto _calc_view()const noexcept -> glm::mat4 override;

		private:
			glm::vec3 _pos;
			glm::quat _rotation;
	};


	class Camera_sidescroller : public Camera {
		public:
			Camera_sidescroller(const glm::vec4& viewport, Angle fov,
			                    Distance dist_min, Distance dist_max) noexcept;

			void move     (Position offset) noexcept {
				position(_pos+offset);
			}
			void position (Position pos) noexcept {
				_pos = pos;
				_set_dirty();
			}
			auto position ()const noexcept {return _pos;}

			auto eye_position() const noexcept -> glm::vec3 override {
				return remove_units(_pos);
			}

			using Camera::screen_to_world;

		private:
			auto _calc_view()const noexcept -> glm::mat4 override;

			Position _pos;
	};

}
}
