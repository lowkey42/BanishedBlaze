/** Wrapper for uniform buffer objects ***************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "../utils/log.hpp"
#include "../utils/func_traits.hpp"

#include <cstdint>


namespace lux {
namespace graphic {

	class Uniform_buffer {
		public:
			Uniform_buffer() : _handle(0), _size(0) {}
			Uniform_buffer(std::size_t size, const void* data=nullptr);
			template<class T>
			Uniform_buffer(const T& uniform_data) : Uniform_buffer(sizeof(T), &uniform_data) {}
			Uniform_buffer(Uniform_buffer&&)noexcept;
			Uniform_buffer& operator=(Uniform_buffer&&)noexcept;
			~Uniform_buffer();
			
			void bind(int slot)const;
			
			template<class T>
			void set(const T& uniform_data);
			
			template<class T, class F>
			void update(F&& callback) {
				callback(*reinterpret_cast<T*>(_map()));
				_unmap();
			}
			template<class F>
			void update(F&& callback) {
				update<std::remove_reference_t<util::nth_func_arg_t<F,0>>>(std::forward<F>(callback));
			}
			
			template<class M>
			void set_member(std::size_t offset, const M& member_data);
			
		private:
			unsigned int _handle;
			std::size_t  _size;
			
			void _set(std::size_t offset, std::size_t size, const void* data);
			auto _map() -> void*;
			void _unmap();
	};

	
	// IMPL
	template<class T>
	void Uniform_buffer::set(const T& uniform_data) {
		if(sizeof(T)!=_size)
		INVARIANT(sizeof(T)==_size, "Sizes used in set and constructor of Uniform_map don't match!");
		_set(0, sizeof(T), &uniform_data);
	}
	
	template<class M>
	void Uniform_buffer::set_member(std::size_t offset, const M& member_data) {
		INVARIANT(offset+sizeof(member_data)<=_size, "Sizes used in set and constructor of Uniform_map don't match!");
		
		_set(offset, sizeof(M), &member_data);
	}
}
}
