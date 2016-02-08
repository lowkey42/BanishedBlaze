#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "vertex_object.hpp"

#include "../utils/log.hpp"

#include "shader.hpp"

namespace mo {
namespace renderer {

	namespace {
		GLenum to_gl(Vertex_layout::Mode m) {
			switch(m) {
				case Vertex_layout::Mode::points:         return GL_POINTS;
				case Vertex_layout::Mode::line_strip:     return GL_LINE_STRIP;
				case Vertex_layout::Mode::line_loop:      return GL_LINE_LOOP;
				case Vertex_layout::Mode::lines:          return GL_LINES;
				case Vertex_layout::Mode::triangle_strip: return GL_TRIANGLE_STRIP;
				case Vertex_layout::Mode::triangle_fan:   return GL_TRIANGLE_FAN;
				case Vertex_layout::Mode::triangles:      return GL_TRIANGLES;
				default: FAIL("Unsupported VertexLayout::Mode");
				return 0;
			}
		}
		GLenum to_gl(Vertex_layout::Element_type e) {
			switch(e) {
				case Vertex_layout::Element_type::byte_t:   return GL_BYTE;
				case Vertex_layout::Element_type::ubyte_t:  return GL_UNSIGNED_BYTE;
				case Vertex_layout::Element_type::short_t:  return GL_SHORT;
				case Vertex_layout::Element_type::ushort_t: return GL_UNSIGNED_SHORT;
				case Vertex_layout::Element_type::int_t:    return GL_INT;
				case Vertex_layout::Element_type::uint_t:   return GL_UNSIGNED_INT;
				case Vertex_layout::Element_type::float_t:  return GL_FLOAT;
				default: FAIL("Unsupported VertexLayout::ElementType");
				return 0;
			}
		}
	}

	Buffer::Buffer(std::size_t element_size, std::size_t elements, bool dynamic,
	               const void* data)
	    : _id(0),
	      _element_size(element_size),
	      _elements(elements),
	      _max_elements(elements),
	      _dynamic(dynamic) {
		glGenBuffers(1, &_id);
		glBindBuffer(GL_ARRAY_BUFFER, _id);
		glBufferData(GL_ARRAY_BUFFER, _elements*_element_size, data,
		             _dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	}
	Buffer::Buffer(Buffer&& b)noexcept
	    : _id(b._id), _element_size(b._element_size),
	      _elements(b._elements), _max_elements(b._elements), _dynamic(b._dynamic) {
		b._id = 0;
	}

	Buffer::~Buffer()noexcept {
		if(_id)
			glDeleteBuffers(1, &_id);
	}

	Buffer& Buffer::operator=(Buffer&& b)noexcept {
		INVARIANT(this!=&b, "move to self");

		if(_id)
			glDeleteBuffers(1, &_id);

		_id = b._id;
		b._id = 0;
		_element_size = b._element_size;
		_elements = b._elements;
		_dynamic = b._dynamic;

		return *this;
	}

	void Buffer::_set_raw(std::size_t element_size, std::size_t elements, const void* data) {
		INVARIANT(element_size==_element_size, "Changeing element size is forbidden!");
		INVARIANT(_dynamic, "set(...) is only allowed for dynamic buffers!");
		INVARIANT(_id!=0, "Can't access invalid buffer!");

		_elements = elements;

#ifdef EMSCRIPTEN
		// webgl on android sucks balls and wierds out if a VBO is updated.
		// So we create a new one for every update, because who realy cares about performance m(
		glDeleteBuffers(1, &_id);
		glGenBuffers(1, &_id);
		glBindBuffer(GL_ARRAY_BUFFER, _id);
		glBufferData(GL_ARRAY_BUFFER, elements*_element_size, data,
		             GL_DYNAMIC_DRAW);
		_max_elements = elements;

#else
		glBindBuffer(GL_ARRAY_BUFFER, _id);

		if(_max_elements>=elements) {
			glBufferData(GL_ARRAY_BUFFER, _max_elements*_element_size, nullptr,
						 GL_STREAM_DRAW);

			glBufferSubData(GL_ARRAY_BUFFER, 0,
							elements*_element_size,
							data);
		} else {
			_max_elements = elements;
			glBufferData(GL_ARRAY_BUFFER, elements*_element_size, data,
			             GL_DYNAMIC_DRAW);
		}
#endif
	}

	void Buffer::_bind()const {
		glBindBuffer(GL_ARRAY_BUFFER, _id);
	}


	void Vertex_layout::setup_shader(Shader_program& shader)const {
		int index = 0;

		for(auto& e : _elements) {
			shader.bind_attribute_location(e.name, index++);
		}
	}
	bool Vertex_layout::_build(const std::vector<Buffer>& buffers)const {
		bool instanced = false;

		std::size_t bound_buffer =-1;
		const Buffer* buffer=nullptr;
		int index = 0;
		for(auto& e : _elements) {
			if(bound_buffer!=e.buffer) {
				bound_buffer = e.buffer;
				buffer = &buffers.at(e.buffer);
				buffer->_bind();
			}

			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index,e.size,to_gl(e.type),e.normalized, buffer->_element_size, e.offset);

#ifdef ANDROID
			INVARIANT(e.divisor==0, "Instancing is not supported on Android devises");
#else
			glVertexAttribDivisor(index, e.divisor);
			instanced |= e.divisor>0;
#endif

			index++;
		}

		return instanced;
	}

	Object::Object(Object&& o)noexcept
	    : _layout(o._layout), _mode(o._mode), _data(std::move(o._data)), _vao_id(o._vao_id) {
		o._vao_id = 0;
	}


#if defined(ANDROID) || defined(EMSCRIPTEN)
	void Object::_init(const Vertex_layout& layout) {
		_layout = &layout;
	}
	Object::~Object()noexcept {
	}

	void Object::draw()const {
		_layout->_build(_data);
		glDrawArrays(to_gl(_mode), 0, _data.at(0).size());
	}

	Object& Object::operator=(Object&& o)noexcept {
		INVARIANT(this!=&o, "move to self");

		_vao_id = o._vao_id;
		o._vao_id = 0;

		_layout = o._layout;
		_mode = o._mode;
		_data = std::move(o._data);
		_instanced = o._instanced;

		return *this;
	}

#else
	void Object::_init(const Vertex_layout& layout) {
		_layout = &layout;

		glGenVertexArrays(1, &_vao_id);

		glBindVertexArray(_vao_id);
		_instanced = layout._build(_data);
		glBindVertexArray(0);
	}
	Object::~Object()noexcept {
		if(_vao_id)
			glDeleteVertexArrays(1, &_vao_id);
	}

	void Object::draw()const {
		glBindVertexArray(_vao_id);
		if(!_instanced)
			glDrawArrays(to_gl(_mode), 0, _data.at(0).size());
		else
			glDrawArraysInstanced(to_gl(_mode), 0, _data.at(0).size(), _data.at(1).size());

		glBindVertexArray(0);
	}

	Object& Object::operator=(Object&& o)noexcept {
		INVARIANT(this!=&o, "move to self");

		if(_vao_id)
			glDeleteVertexArrays(1, &_vao_id);

		_vao_id = o._vao_id;
		o._vao_id = 0;

		_layout = o._layout;
		_mode = o._mode;
		_data = std::move(o._data);
		_instanced = o._instanced;

		return *this;
	}
#endif

}
}
