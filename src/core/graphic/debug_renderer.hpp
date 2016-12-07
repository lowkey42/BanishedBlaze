/** debug renderer for framebuffers and textures *****************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

namespace lux {
namespace asset {
	class Asset_manager;
}
namespace graphic {
	class Framebuffer;
	class Texture;
	class Graphics_ctx;

	extern void debug_draw_framebuffer(Framebuffer&);
	extern void debug_draw_texture(Texture&);

	extern void init_debug_renderer(asset::Asset_manager&, Graphics_ctx&);
	extern void on_frame_debug_draw();

}
}
