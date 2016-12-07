/** simple text renderer (bitmap fonts) **************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "texture.hpp"
#include "vertex_object.hpp"
#include "shader.hpp"
#include "primitives.hpp"

#include "../asset/asset_manager.hpp"


namespace lux {
namespace graphic {

	class Command_queue;
	class Text;
	using Text_ptr = std::shared_ptr<const Text>;

	using Text_char = uint32_t;

	struct Glyph {
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		int offset_x = 0;
		int offset_y = 0;
		int advance = 0;
		std::unordered_map<Text_char, int> kerning;
	};

	/*
	 * Format:
	 * family
	 * size height
	 * texture
	 * symbol_count
	 * id x y width height offset_x offset_y advance
	 * ...
	 * kerning_count
	 * id prev_char value
	 * ...
	 */
	class Font : public std::enable_shared_from_this<Font> {
		public:
			Font(asset::Asset_manager& assets, std::istream&);

			auto text(const std::string& str)const -> Text_ptr;
			auto calculate_size(const std::string& str)const -> glm::vec2;

		private:
			friend class Text;
			friend class Text_dynamic;

			void calculate_vertices(const std::string& str, std::vector<Simple_vertex>& out,
			                        bool monospace=false)const;

			int _height = 0;
			int _line_height = 0;
			Texture_ptr _texture;
			std::unordered_map<Text_char, Glyph> _glyphs;

			mutable std::unordered_map<std::string, Text_ptr> _cache;
	};
	using Font_ptr = asset::Ptr<Font>;
	using Font_sptr = std::shared_ptr<const graphic::Font>;

	class Text {
		public:
			Text(Font_sptr font, std::vector<Simple_vertex> vertices);

			void draw(Command_queue&, glm::vec2 center, glm::vec4 color={1,1,1,1},
			          float scale=1)const;

			auto size()const noexcept {return _size;}

		protected:
			Font_sptr _font;
			Object _obj;
			glm::vec2 _size;
	};

	class Text_dynamic {
		public:
			Text_dynamic(Font_sptr font);

			void draw(Command_queue&, glm::vec2 center, glm::vec4 color={1,1,1,1},
			          float scale=1)const;

			void set(const std::string& str, bool monospace=false);

			auto size()const noexcept {return _size;}
			operator bool()const {return !_data.empty();}

		protected:
			Font_sptr _font;
			std::vector<Simple_vertex> _data;
			Object _obj;
			glm::vec2 _size;
	};

	extern void init_font_renderer(asset::Asset_manager&);
}

namespace asset {
	template<>
	struct Loader<graphic::Font> {
		using RT = std::shared_ptr<graphic::Font>;

		static RT load(istream in) throw(Loading_failed){
			return std::make_shared<graphic::Font>(in.manager(), in);
		}

		static void store(ostream out, const graphic::Texture& asset) throw(Loading_failed) {
			// TODO
			FAIL("NOT IMPLEMENTED, YET!");
		}
	};
}
}
