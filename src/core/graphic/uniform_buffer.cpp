#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "uniform_buffer.hpp"

#include <cstring>


namespace lux {
namespace graphic {

	Uniform_buffer::Uniform_buffer(std::size_t size, const void* data)
	    : _handle(0), _size(size) {
		
		glGenBuffers(1, &_handle);
		glBindBuffer(GL_UNIFORM_BUFFER, _handle);
		glBufferData(GL_UNIFORM_BUFFER, _size, data, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	
	Uniform_buffer::Uniform_buffer(Uniform_buffer&& rhs)noexcept
	    : _handle(rhs._handle), _size(rhs._size) {
		
		rhs._handle = 0;
	}
	Uniform_buffer& Uniform_buffer::operator=(Uniform_buffer&& rhs)noexcept {
		INVARIANT(this!=&rhs, "Self move assignment");
		
		if(_handle) {
			glDeleteBuffers(1, &_handle);
		}
		
		_handle = rhs._handle;
		_size = rhs._size;
		
		rhs._handle = 0;
		
		return *this;
	}
	Uniform_buffer::~Uniform_buffer() {
		if(_handle)
			glDeleteBuffers(1, &_handle);
	}
	
	void Uniform_buffer::_set(std::size_t offset, std::size_t size, const void* data) {
		glBindBuffer(GL_UNIFORM_BUFFER, _handle);
		auto ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
		memcpy(((char*)ptr)+offset, data, size);
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}
	auto Uniform_buffer::_map() -> void* {
		glBindBuffer(GL_UNIFORM_BUFFER, _handle);
		return glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
	}
	void Uniform_buffer::_unmap() {
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}
	
	void Uniform_buffer::bind(int slot)const {
		glBindBufferBase(GL_UNIFORM_BUFFER, slot, _handle);
	}

}
}
