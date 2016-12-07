#define GLM_SWIZZLE

#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif


#include "camera.hpp"

#include "graphics_ctx.hpp"

#include "../engine.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <SDL2/SDL.h>

#include <iostream>


namespace lux {
namespace graphic {

	using namespace unit_literals;

	glm::vec2 calculate_vscreen(const Engine& engine, int target_height) {
		float width = engine.graphics_ctx().viewport().z;
		float height = engine.graphics_ctx().viewport().w;

		if(width/static_cast<float>(height) <= 5/3.f) { // special case for weird resolutions
			target_height*=1.25f;
		}

		if(width < height) { // special case for portrait-mode
			float vheight  = std::round(height * (static_cast<float>(target_height))/width);
			return {target_height, vheight};

		} else {
			float vwidth  = std::round(width * (static_cast<float>(target_height))/height);
			return {vwidth, target_height};
		}
	}

	namespace {
		auto calc_projection(int width, int height) -> glm::mat4 {
			return glm::ortho(-width/2.f, width/2.f,
							  height/2.f, -height/2.f, -1.0f, 1.0f);
		}
		auto aspect_radio(const glm::vec4& viewport) {
			return (viewport.z-viewport.x) / (viewport.w-viewport.y);
		}
	}

	Camera::Camera(const glm::vec4& viewport, glm::mat4 proj)
	    : _viewport(viewport), _projection(proj) {
	}

	auto Camera::screen_to_world(glm::vec2 screen_pos, float depth) const noexcept
	                             -> glm::vec3 {

		screen_pos.y = _viewport.w-screen_pos.y;

		return glm::unProject(glm::vec3(screen_pos,depth), glm::mat4(1),
		                      vp(), _viewport);
	}
	auto Camera::world_to_screen(glm::vec3 world_pos) const noexcept
	                             -> glm::vec2 {

		auto sp = glm::project(world_pos, glm::mat4(1), vp(), _viewport).xy();
		sp.y = _viewport.w-sp.y;
		return sp;
	}

	auto Camera::screen_to_world(glm::vec2 screen_pos, glm::vec3 expected_pos) const noexcept -> glm::vec3 {
		auto depth = glm::project(expected_pos, glm::mat4(1), vp(), viewport()).z;
		return screen_to_world(screen_pos, depth);
	}


	Camera_2d::Camera_2d(const glm::vec4& viewport,
	                     glm::vec2 size, float world_scale,
	                     glm::vec2 position, float zoom) noexcept
	    : Camera(viewport, calc_projection(size.x, size.y)),
	      _size(size),
	      _world_scale(world_scale),
	      _pos(position),
	      _zoom(zoom) {
	}

	auto Camera_2d::area()const noexcept -> glm::vec4 {
		auto start = screen_to_world(viewport().xy()).xy() - glm::vec2(0.5f, 0.5f);
		auto end   = screen_to_world(viewport().xy() +
									 viewport().zw()).xy() + glm::vec2(1.5f, 1.5f);

		return {start.x, start.y, end.x, end.y};
	}

	auto Camera_2d::_calc_view()const noexcept -> glm::mat4 {
		auto z = _zoom*_world_scale;

		auto scale = glm::vec3(z, z, 1.f);

		auto translation = glm::vec3(
				-std::round(_pos.x*z)/z,
				-std::round(_pos.y*z)/z, 0);

		return glm::translate(glm::scale(glm::mat4(1.0f), scale), translation);
	}



	Camera_3d::Camera_3d(const glm::vec4& viewport, Angle fov, glm::vec3 pos,
	                     glm::vec3 look_at_target, float near, float far) noexcept
	    : Camera(viewport, glm::perspective(fov.value(),
	                                        aspect_radio(viewport), near, far)),
	      _pos(pos) {

		look_at(look_at_target);
	}

	void Camera_3d::rotation(glm::quat rot) noexcept {
		_rotation = glm::normalize(rot);
		_set_dirty();
	}
	void Camera_3d::rotate_local (float roll, float pitch,
	                              float yaw) noexcept {
		auto w = glm::quat(0,roll,pitch,yaw);
		rotation(rotation() + 0.5f*rotation()*w);
	}
	void Camera_3d::rotate_global(float roll, float pitch,
	                              float yaw) noexcept {
		auto w = glm::quat(0,roll,pitch,yaw);
		rotation(rotation() + 0.5f*w*rotation());
	}
	void Camera_3d::look_at      (glm::vec3 target)   noexcept {
		auto dir = target-position();
		auto up  = glm::length2(glm::vec3(0,1,0)-dir)<0.2 ? glm::vec3(0,0,1) : glm::vec3(0,1,0);

		auto rot = glm::lookAt(position(), target, up);
		rotation(glm::quat_cast(rot));
	}

	auto Camera_3d::_calc_view()const noexcept -> glm::mat4 {
		auto view = glm::mat4(1);
		view[3].x = -_pos.x;
		view[3].y = -_pos.y;
		view[3].z = -_pos.z;

		return glm::toMat4(_rotation) * view;
	}


	Camera_sidescroller::Camera_sidescroller(const glm::vec4& viewport, Angle fov,
	                  Distance dist_min, Distance dist_max) noexcept
	    : Camera(viewport, glm::perspective(fov.value(),
	                                        aspect_radio(viewport),
	                                        1.0f,
	                                        dist_min.value() + dist_max.value() +0.2f)),
	      _pos(0_m, 0_m, dist_min + 0.1f) {
	}

	namespace {
		auto& sidescrol_look_mat() {
			static auto mat = glm::lookAt(glm::vec3(0,0,5), glm::vec3(0,0,0), glm::vec3(0,1,0));
			return mat;
		}
	}
	auto Camera_sidescroller::_calc_view()const noexcept -> glm::mat4 {
		auto view = glm::mat4(1);
		view[3].x = -_pos.x.value();
		view[3].y = -_pos.y.value();
		view[3].z = -_pos.z.value();

		return sidescrol_look_mat() * view;
	}
}
}
