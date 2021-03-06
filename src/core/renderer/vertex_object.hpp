/** Wrappers for VAOs & VBOs *************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <cinttypes>
#include <vector>
#include <iterator>
#include <string>
#include <glm/glm.hpp>

#include "../utils/template_utils.hpp"
#include "../utils/maybe.hpp"

/*
 * Examples:
	struct FancyData {
		glm::vec2 pos;
		bool isMagic;
	};

	std::vector<FancyData> data;

	auto buffer = createBuffer(data, true);
	Vertex_layout layout{
		Vertex_layout::Mode::triangles,
		vertex("position",  &FancyData::pos),
		vertex("magic",     &FancyData::isMagic)
	};

	auto shader = Shader_program{}
		.attachShader(assetMgr.load<Shader>("frag_shader:example"_aid))
		.attachShader(assetMgr.load<Shader>("vert_shader:example"_aid))
		.bindAllAttributeLocations(layout)
		.build();

	Object obj{layout, {buffer}};

	shader.set_uniform("liveMagic", 0.42f)
	      .bind();

	obj.draw();
	data.push_back({1,2,3,true}); //< update data
	obj.getBuffer().set(data);

	obj.draw(); //< draws updated data
 */
namespace lux {
namespace renderer {

	class Object;
	class Shader_program;

	enum class Index_type {
		unsigned_byte,
		unsigned_short,
		unsigned_int
	};

	class Buffer : util::no_copy {
		friend class Object;
		friend class Vertex_layout;
		public:
			Buffer(std::size_t element_size, std::size_t elements,
			       bool dynamic, const void* data=nullptr, bool index_buffer=false);
			Buffer(Buffer&& b)noexcept;
			~Buffer()noexcept;

			template<class T>
			void set(const std::vector<T>& container);

			template<class Iter>
			void set(Iter begin, Iter end);

			Buffer& operator=(Buffer&& b)noexcept;

			std::size_t size()const noexcept{return _elements;}

			auto index_buffer()const noexcept {return _index_buffer;}
			auto index_buffer_type()const noexcept {return _index_buffer_type;}

		private:
			unsigned int _id;
			std::size_t _element_size;
			std::size_t _elements;
			std::size_t _max_elements;
			bool _dynamic;
			bool _index_buffer; //< GL_ELEMENT_ARRAY_BUFFER
			Index_type _index_buffer_type = Index_type::unsigned_byte;

			void _set_raw(std::size_t element_size, std::size_t size, const void* data);
			void _bind()const;
	};
	template<class T>
	Buffer create_dynamic_buffer(std::size_t elements, bool index_buffer=false);

	template<class T>
	Buffer create_buffer(const std::vector<T>& container, bool dynamic=false, bool index_buffer=false);

	template<class T>
	Buffer create_buffer(typename std::vector<T>::const_iterator begin,
	                     typename std::vector<T>::const_iterator end,
	                     bool dynamic=false, bool index_buffer=false);


	class Vertex_layout {
		friend class Object;
		public:
			enum class Mode {
				points,
				line_strip,
				line_loop,
				lines,
				triangle_strip,
				triangle_fan,
				triangles
			};

			enum class Element_type {
				byte_t, ubyte_t, short_t, ushort_t, int_t, uint_t, float_t
			};

			struct Element {
				std::string name;
				int size;
				Element_type type;
				bool normalized;
				const void* offset;
				std::size_t buffer;
				uint8_t divisor;
			};

		public:
			template<typename ...T>
			Vertex_layout(Mode mode, T&&... elements)
			    : _mode(mode), _elements{std::forward<T>(elements)...} {}

			void setup_shader(Shader_program& shader)const;


		private:
			const Mode _mode;
			const std::vector<Element> _elements;

			/// returns ture for instanced rendering
			bool _build(const std::vector<Buffer>& buffers)const;
	};
	template<class Base> Vertex_layout::Element vertex(const std::string& name, int8_t    Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, uint8_t   Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, int16_t   Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, uint16_t  Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, int32_t   Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, uint32_t  Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, float     Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, double    Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, glm::vec2 Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, glm::vec3 Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);
	template<class Base> Vertex_layout::Element vertex(const std::string& name, glm::vec4 Base::* value, std::size_t buffer=0, uint8_t divisor=0, bool normalized=false);


	class Object : util::no_copy {
		public:
			template<class... B>
			Object(const Vertex_layout& layout, B&&... data);
			Object(Object&&)noexcept;
			~Object()noexcept;

			void draw(int offset=0, int count=-1)const;

			auto& buffer(std::size_t i=0){return _data.at(i);}
			auto& index_buffer() {return _index_buffer;}

			Object& operator=(Object&&)noexcept;

		private:
			void _init(const Vertex_layout& layout);

			const Vertex_layout* _layout;
			Vertex_layout::Mode _mode;
			std::vector<Buffer> _data;
			util::maybe<Buffer> _index_buffer = util::nothing();
			unsigned int _vao_id;
			bool _instanced;
	};

}
}

#define VERTEX_OBJECT_INCLUDED
#include "vertex_object.hxx"

