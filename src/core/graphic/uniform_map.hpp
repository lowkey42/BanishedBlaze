/** fire and forget storage for a fixed number of uniforms *******************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "shader.hpp"

#include "../utils/log.hpp"
#include "../utils/func_traits.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>


namespace lux {
namespace graphic {

	enum class Uniform_type : unsigned int {
		integer        = sizeof(int)      <<4 | 0x0,
		floating_point = sizeof(float)*1  <<4 | 0x1,
		fvec2          = sizeof(float)*2  <<4 | 0x2,
		fvec3          = sizeof(float)*3  <<4 | 0x3,
		fvec4          = sizeof(float)*4  <<4 | 0x4,
		fmat2          = sizeof(float)*2*2<<4 | 0x5,
		fmat3          = sizeof(float)*3*3<<4 | 0x6,
		fmat4          = sizeof(float)*4*4<<4 | 0x7
	};
	constexpr auto uniform_size(Uniform_type type) {
		return static_cast<unsigned int>(type) >> 4;
	}

	struct IUniform_map {
		virtual ~IUniform_map() = default;

		virtual auto emplace(const char* name,       float      val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name,       int        val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name,       glm::vec2  val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name,       glm::vec3  val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name,       glm::vec4  val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name, const glm::mat2& val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name, const glm::mat3& val) -> IUniform_map& = 0;
		virtual auto emplace(const char* name, const glm::mat4& val) -> IUniform_map& = 0;

		virtual void clear() = 0;

		virtual void bind_all(Shader_program&)const = 0;
	};

	/// provies enough slots for up to 'max_size' uniforms and storage for
	/// a combined size of 'max_size'*'average_size'
	template<std::size_t max_slots, std::size_t average_size=sizeof(float)*4>
	class Uniform_map : public IUniform_map {
		public:
			using this_t = Uniform_map<max_slots, average_size>;

			auto emplace(const char* name,       float      val) -> this_t& override final;
			auto emplace(const char* name,       int        val) -> this_t& override final;
			auto emplace(const char* name,       glm::vec2  val) -> this_t& override final;
			auto emplace(const char* name,       glm::vec3  val) -> this_t& override final;
			auto emplace(const char* name,       glm::vec4  val) -> this_t& override final;
			auto emplace(const char* name, const glm::mat2& val) -> this_t& override final;
			auto emplace(const char* name, const glm::mat3& val) -> this_t& override final;
			auto emplace(const char* name, const glm::mat4& val) -> this_t& override final;
			void clear() override final;

			void bind_all(Shader_program&)const override final;

		private:
			struct Uniform {
				const char* name;
				Uniform_type type;
			};

			std::array<Uniform, max_slots> _metadata;
			std::array<float, max_slots*average_size> _data;
			unsigned int _next_metadata = 0;
			unsigned int _next_data = 0;

			template<Uniform_type type>
			int _get_slot(const char* name)noexcept;

			template<Uniform_type type>
			int _reserve(const char* name)noexcept;

			template<Uniform_type type>
			int _find_existing(const char* name)noexcept;
	};


	namespace detail {
		constexpr auto sum() -> std::size_t {
			return 0;
		}
		template<class FirstArg, class... Args>
		constexpr auto sum(FirstArg first, Args... args) -> std::size_t {
			return first + sum(args...);
		}

		constexpr auto sum_even() -> std::size_t {
			return 0;
		}
		template<class ArgA, class ArgB, class... Args>
		constexpr auto sum_even(ArgA, ArgB b, Args... args) -> std::size_t {
			return b + sum_even(args...);
		}
		template<class T>
		constexpr auto my_sizeof() -> std::size_t {
			return sizeof(T);
		}
	}

	template<class... Args>
	auto make_uniform_map(Args&&... args) -> std::unique_ptr<IUniform_map> {
		constexpr auto max_slots = sizeof...(args)/2;
		constexpr auto sum_size = detail::sum_even(detail::my_sizeof<Args>()...);
		constexpr auto average_size = static_cast<std::size_t>(sum_size*1.f / max_slots + 0.5f);
		auto m = std::unique_ptr<IUniform_map>(new Uniform_map<max_slots, average_size>);

		util::apply2([&m](auto&& name, auto&& value){
			m->emplace(std::forward<decltype(name)>(name), std::forward<decltype(value)>(value));

		}, std::forward<Args>(args)...);

		return m;
	}


	// impl
	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, float val) -> this_t& {
		_data.at(_get_slot<Uniform_type::floating_point>(name)) = val;
		return *this;
	}

	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, int val) -> this_t& {
		_data.at(_get_slot<Uniform_type::integer>(name)) = static_cast<float>(val);
		return *this;
	}

	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, glm::vec2 val) -> this_t& {
		auto index = _get_slot<Uniform_type::fvec2>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 2*sizeof(float));
		return *this;
	}

	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, glm::vec3 val) -> this_t& {
		auto index = _get_slot<Uniform_type::fvec3>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 3*sizeof(float));
		return *this;
	}
	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, glm::vec4 val) -> this_t& {
		auto index = _get_slot<Uniform_type::fvec4>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 4*sizeof(float));
		return *this;
	}
	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, const glm::mat2& val) -> this_t& {
		auto index = _get_slot<Uniform_type::fmat2>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 2*2*sizeof(float));
		return *this;
	}
	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, const glm::mat3& val) -> this_t& {
		auto index = _get_slot<Uniform_type::fmat3>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 3*3*sizeof(float));
		return *this;
	}
	template<std::size_t max_slots, std::size_t average_size>
	auto Uniform_map<max_slots,average_size>::emplace(const char* name, const glm::mat4& val) -> this_t& {
		auto index = _get_slot<Uniform_type::fmat4>(name);
		std::memcpy(&_data.at(index), glm::value_ptr(val), 4*4*sizeof(float));
		return *this;
	}


	template<std::size_t max_slots, std::size_t average_size>
	template<Uniform_type type>
	int Uniform_map<max_slots,average_size>::_get_slot(const char* name)noexcept {
		int slot = _find_existing<type>(name);
		if(slot>=0)
			return slot;

		else return _reserve<type>(name);
	}

	template<std::size_t max_slots, std::size_t average_size>
	template<Uniform_type type>
	int Uniform_map<max_slots,average_size>::_reserve(const char* name)noexcept {
		constexpr auto size = uniform_size(type) / sizeof(float);

		INVARIANT(_next_data+size<=_data.size(), "Not enough space in uniform map");
		INVARIANT(_next_metadata<_metadata.size(), "Not enough slots in uniform map");

		auto& metadata = _metadata.at(_next_metadata++);
		metadata.name = name;
		metadata.type = type;

		auto start_idx = _next_data;
		_next_data += size;
		return start_idx;
	}

	template<std::size_t max_slots, std::size_t average_size>
	template<Uniform_type type>
	int Uniform_map<max_slots,average_size>::_find_existing(const char* name)noexcept {
		auto search = [&](auto&& comp) {
			auto data_idx = 0;

			for(auto i=0u; i<_next_metadata; ++i) {
				if(comp(name, _metadata[i].name)) {
					INVARIANT(_metadata[i].type==type, "Found uniform with same name but different type: "<<((int)_metadata[i].type)<<" vs "<<((int)type));
					return data_idx;
				}

				data_idx+=uniform_size(_metadata[i].type) / sizeof(float);
			}

			return -1;
		};

		// search by address
		auto data_idx = search([](auto a, auto b){return a==b;});

		// search by value
		if(data_idx<0)
			data_idx = search([](auto a, auto b){return std::strcmp(a,b)==0;});

		return data_idx;
	}

	template<std::size_t max_slots, std::size_t average_size>
	void Uniform_map<max_slots,average_size>::clear() {
		_next_data = 0;
		_next_metadata = 0;
	}

	template<std::size_t max_slots, std::size_t average_size>
	void Uniform_map<max_slots,average_size>::bind_all(Shader_program& prog)const {
		auto i=0u;
		auto data_idx=0u;
		for(auto& metadata : _metadata) {
			if(i++ >=_next_metadata)
				break;

			switch(metadata.type) {
				case Uniform_type::integer:
					prog.set_uniform(metadata.name, static_cast<int>(_data.at(data_idx)));
					data_idx += 1;
					break;

				case Uniform_type::floating_point:
					prog.set_uniform(metadata.name, static_cast<float>(_data.at(data_idx)));
					data_idx += 1;
					break;

				case Uniform_type::fvec2:
					prog.set_uniform(metadata.name, glm::make_vec2(&_data.at(data_idx)));
					data_idx += 2;
					break;

				case Uniform_type::fvec3:
					prog.set_uniform(metadata.name, glm::make_vec3(&_data.at(data_idx)));
					data_idx += 3;
					break;

				case Uniform_type::fvec4:
					prog.set_uniform(metadata.name, glm::make_vec4(&_data.at(data_idx)));
					data_idx += 4;
					break;

				case Uniform_type::fmat2:
					prog.set_uniform(metadata.name, glm::make_mat2(&_data.at(data_idx)));
					data_idx += 2*2;
					break;

				case Uniform_type::fmat3:
					prog.set_uniform(metadata.name, glm::make_mat3(&_data.at(data_idx)));
					data_idx += 3*3;
					break;

				case Uniform_type::fmat4:
					prog.set_uniform(metadata.name, glm::make_mat4(&_data.at(data_idx)));
					data_idx += 4*4;
					break;

				default: FAIL("Unexpected Uniform_type: "<<(int)metadata.type);
			}
		}
	}

}
}
