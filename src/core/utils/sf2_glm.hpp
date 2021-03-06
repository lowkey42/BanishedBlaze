/** SF2 annotations for glm types ********************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <sf2/sf2.hpp>

namespace glm {
namespace detail {

	inline void load(sf2::JsonDeserializer& s, vec2& v) {
		s.read_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y),
			sf2::vmember("w", v.x),
			sf2::vmember("h", v.y)
		);
	}
	inline void save(sf2::JsonSerializer& s, const vec2& v) {
		s.write_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y)
		);
	}

	inline void load(sf2::JsonDeserializer& s, vec3& v) {
		s.read_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y),
			sf2::vmember("z", v.z),

			sf2::vmember("r", v.x),
			sf2::vmember("g", v.y),
			sf2::vmember("b", v.z)
		);
	}
	inline void save(sf2::JsonSerializer& s, const vec3& v) {
		s.write_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y),
			sf2::vmember("z", v.z)
		);
	}

	inline void load(sf2::JsonDeserializer& s, vec4& v) {
		s.read_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y),
			sf2::vmember("z", v.z),
			sf2::vmember("w", v.w),

			sf2::vmember("r", v.x),
			sf2::vmember("g", v.y),
			sf2::vmember("b", v.z),
			sf2::vmember("a", v.a)
		);
	}
	inline void save(sf2::JsonSerializer& s, const vec4& v) {
		s.write_virtual(
			sf2::vmember("x", v.x),
			sf2::vmember("y", v.y),
			sf2::vmember("z", v.z),
			sf2::vmember("w", v.w)
		);
	}


	inline void load(sf2::JsonDeserializer& s, quat& v) {
		float r,p,y;

		s.read_virtual(
			sf2::vmember("roll", r),
			sf2::vmember("pitch", p),
			sf2::vmember("yaw", y)
		);

		v = quat(glm::vec3(r,p,y));
	}
	inline void save(sf2::JsonSerializer& s, const quat& v) {
		float r = roll(v);
		float p = pitch(v);
		float y = yaw(v);

		s.write_virtual(
			sf2::vmember("roll", r),
			sf2::vmember("pitch", p),
			sf2::vmember("yaw", y)
		);
	}

}
}
