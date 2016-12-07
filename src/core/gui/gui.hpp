/** Manager for nuklear ui ***************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include <nuklear.h>

#include "../graphic/command_queue.hpp"


struct nk_context;


namespace lux {
	class Engine;

namespace gui {

	// TODO: gamepad input: https://gist.github.com/vurtun/519801825b4ccfad6767
	//                      https://github.com/vurtun/nuklear/issues/50
	// TODO: theme support: https://github.com/vurtun/nuklear/blob/master/demo/style.c
	//                      https://github.com/vurtun/nuklear/blob/master/example/skinning.c
	// TODO: merge fixes in nuklear.h back into upstream
	class Gui {
		public:
			Gui(Engine& engine);
			~Gui();

			// ignores the Command_queue and depth-buffer
			void draw();

			auto ctx() -> nk_context*;

			auto centered(int width, int height) -> struct nk_rect;


		private:
			struct PImpl;
			std::unique_ptr<PImpl> _impl;
	};


	// widgets
	extern bool color_picker(nk_context*, Rgb&  color, int width, float factor=1.f);
	extern bool color_picker(nk_context*, Rgba& color, int width, float factor=1.f);

	extern void begin_menu(nk_context*, int& active);
	extern bool menu_button(nk_context*, const char* text, bool enabled=true);
	extern void end_menu(nk_context*);

	class Text_edit {
		public:
			Text_edit();
			Text_edit(Text_edit&&)=default;
			Text_edit& operator=(Text_edit&&)=default;
			~Text_edit();

			void reset(const std::string&);
			void get(std::string&)const;

			auto active()const noexcept {return _active;}

			void update_and_draw(nk_context*, nk_flags type);
			void update_and_draw(nk_context*, nk_flags type, std::string&);

		private:
			util::maybe<nk_text_edit> _data;
			bool _active=false;
	};

}
}
