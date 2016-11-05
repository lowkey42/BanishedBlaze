#define NK_IMPLEMENTATION

#define GLM_SWIZZLE

#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "gui.hpp"

#include "../asset/asset_manager.hpp"
#include "../input/events.hpp"
#include "../input/input_manager.hpp"
#include "../renderer/camera.hpp"
#include "../renderer/graphics_ctx.hpp"
#include "../renderer/shader.hpp"
#include "../renderer/texture.hpp"
#include "../renderer/vertex_object.hpp"

#include <sf2/sf2.hpp>

#include <SDL2/SDL.h>

#include <string>


namespace lux {
namespace renderer {

	// extend vertex for nk_vec2
	template<class Base>
	Vertex_layout::Element vertex(const std::string& name, struct nk_vec2 Base::* value, std::size_t buffer=0,
	                              uint8_t divisor=0, bool normalized=false) {
		return Vertex_layout::Element{name, 2, Vertex_layout::Element_type::float_t, normalized,
		                              details::calcOffset(value), buffer, divisor};
	}
}

namespace gui {

	using namespace renderer;

	namespace {
		struct Font_desc {
			std::string aid;
			float size;
			bool default_font=false;
		};

		struct Gui_cfg {
			std::vector<Font_desc> fonts;
		};
		sf2_structDef(Font_desc, aid, size, default_font)
		sf2_structDef(Gui_cfg, fonts)

		void nk_sdl_clipbard_paste(nk_handle, struct nk_text_edit *edit) {
			auto text = SDL_GetClipboardText();
			if (text) {
				nk_textedit_paste(edit, text, nk_strlen(text));
			}
		}

		void nk_sdl_clipbard_copy(nk_handle, const char *text, int len) {
			if (len<=0) return;

			auto str = std::string(text, static_cast<std::size_t>(len));
			SDL_SetClipboardText(str.c_str());
		}

		auto sdl_to_nk_mb(Uint8 button) -> util::maybe<nk_buttons> {
			switch(button) {
				case SDL_BUTTON_LEFT:   return NK_BUTTON_LEFT;
				case SDL_BUTTON_MIDDLE: return NK_BUTTON_MIDDLE;
				case SDL_BUTTON_RIGHT:  return NK_BUTTON_RIGHT;
				default:
					return util::nothing();
			}
		}

		class Gui_event_filter : public Sdl_event_filter {
			public:
				Gui_event_filter(Engine& e, nk_context* ctx, Camera_2d& camera)
					: Sdl_event_filter(e), _input_mgr(e.input()), _ctx(ctx), _camera(camera) {
				}

				void pre_input_events()override {
					_grab_clicks = nk_window_is_any_hovered(_ctx);
					_grab_inputs = nk_item_is_any_active(_ctx);

					nk_input_begin(_ctx);
				}
				void post_input_events()override {
					nk_input_end(_ctx);
				}

				bool handle_key(bool down, SDL_Keycode key) {
					const auto state = SDL_GetKeyboardState(0);
					const auto ctrl  = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];

					switch(key) {
						case SDLK_RSHIFT:
						case SDLK_LSHIFT:
							nk_input_key(_ctx, NK_KEY_SHIFT, down);
							return false;

						case SDLK_DELETE:
							nk_input_key(_ctx, NK_KEY_DEL, down);
							return false;

						case SDLK_RETURN:
							nk_input_key(_ctx, NK_KEY_ENTER, down);
							return false;

						case SDLK_TAB:
							nk_input_key(_ctx, NK_KEY_TAB, down);
							return false;

						case SDLK_BACKSPACE:
							nk_input_key(_ctx, NK_KEY_BACKSPACE, down);
							return false;

						case SDLK_HOME:
							nk_input_key(_ctx, NK_KEY_TEXT_START, down);
							nk_input_key(_ctx, NK_KEY_SCROLL_START, down);
							return false;
						case SDLK_END:
							nk_input_key(_ctx, NK_KEY_TEXT_END, down);
							nk_input_key(_ctx, NK_KEY_SCROLL_END, down);
							return false;

						case SDLK_PAGEDOWN:
							nk_input_key(_ctx, NK_KEY_SCROLL_DOWN, down);
							return false;
						case SDLK_PAGEUP:
							nk_input_key(_ctx, NK_KEY_SCROLL_UP, down);
							return false;

						case SDLK_z:
							nk_input_key(_ctx, NK_KEY_TEXT_UNDO, down && ctrl);
							return false;
						case SDLK_y:
							nk_input_key(_ctx, NK_KEY_TEXT_REDO, down && ctrl);
							return false;

						case SDLK_c:
							nk_input_key(_ctx, NK_KEY_COPY, down && ctrl);
							return false;
						case SDLK_x:
							nk_input_key(_ctx, NK_KEY_CUT, down && ctrl);
							return false;
						case SDLK_v:
							nk_input_key(_ctx, NK_KEY_PASTE, down && ctrl);
							return false;

						case SDLK_b:
							nk_input_key(_ctx, NK_KEY_TEXT_LINE_START, down && ctrl);
							return false;
						case SDLK_e:
							nk_input_key(_ctx, NK_KEY_TEXT_LINE_END, down && ctrl);
							return false;

						case SDLK_LEFT:
							nk_input_key(_ctx, NK_KEY_TEXT_WORD_LEFT, down && ctrl);
							nk_input_key(_ctx, NK_KEY_LEFT, down && !ctrl);
							return false;

						case SDLK_RIGHT:
							nk_input_key(_ctx, NK_KEY_TEXT_WORD_RIGHT, down && ctrl);
							nk_input_key(_ctx, NK_KEY_RIGHT, down && !ctrl);
							return false;

						default:
							return true;
					}
				}

				bool propagate(SDL_Event& evt) override {
					switch(evt.type) {
						case SDL_KEYUP:
						case SDL_KEYDOWN:
							if(!_grab_inputs)
								return true;

							handle_key(evt.type==SDL_KEYDOWN, evt.key.keysym.sym);
							return evt.key.keysym.sym==SDLK_ESCAPE;

						case SDL_MOUSEBUTTONDOWN:
						case SDL_MOUSEBUTTONUP:
							sdl_to_nk_mb(evt.button.button).process([&](auto button) {
								auto p = _camera.screen_to_world({evt.button.x, evt.button.y});
								nk_input_button(_ctx, button, static_cast<int>(p.x), static_cast<int>(p.y),
								                evt.type==SDL_MOUSEBUTTONDOWN);
							});
							return !_grab_clicks;

						case SDL_MOUSEMOTION: {
							auto p = _camera.screen_to_world({evt.motion.x, evt.motion.y});
							nk_input_motion(_ctx, static_cast<int>(p.x), static_cast<int>(p.y));
							return true; //< never swallow mouse-move
						}

						case SDL_MOUSEWHEEL:
							if(!_grab_clicks)
								return true;

							nk_input_scroll(_ctx, static_cast<float>(evt.wheel.y));
							return false;

						case SDL_TEXTINPUT: {
							if(!_grab_inputs)
								return true;

							nk_glyph glyph;
							memcpy(glyph, evt.text.text, NK_UTF_SIZE);
							nk_input_glyph(_ctx, glyph);
							return false;
						}

						default:
							return true;
					}
				}

			private:
				input::Input_manager& _input_mgr;
				nk_context* _ctx;
				Camera_2d& _camera;
				bool _grab_clicks = false;
				bool _grab_inputs = false;
		};

		struct Wnk_Context {
			nk_context ctx;

			Wnk_Context() {
				nk_init_default(&ctx, nullptr);
				ctx.clip.copy = nk_sdl_clipbard_copy;
				ctx.clip.paste = nk_sdl_clipbard_paste;
				ctx.clip.userdata = nk_handle_ptr(0);
			}
			~Wnk_Context() {
				nk_free(&ctx);
			}
		};
		struct Wnk_Buffer {
			nk_buffer buffer;

			Wnk_Buffer() {
				nk_buffer_init_default(&buffer);
			}
			~Wnk_Buffer() {
				nk_buffer_free(&buffer);
			}
		};
		struct Wnk_font_atlas {
			nk_font_atlas atlas;
			const uint8_t* data;
			int width;
			int height;

			Wnk_font_atlas(Engine& e) {
				nk_font_atlas_init_default(&atlas);
				nk_font_atlas_begin(&atlas);

				e.assets().load_maybe<Gui_cfg>("cfg:gui"_aid).process([&](auto& cfg) {
					for(auto& font : cfg->fonts) {
						auto stream = e.assets().load_raw(font.aid);
						if(stream.is_some()) {
							auto data = stream.get_or_throw().bytes();
							auto f = nk_font_atlas_add_from_memory(&atlas, data.data(), data.size(), font.size, nullptr);
							if(font.default_font) {
								atlas.default_font = f;
							}
							DEBUG("Loaded font \""<<font.aid<<"\" in fontsize "<<font.size);
						}
					}
				});

				data = static_cast<const uint8_t*>(nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32));
			}
			void post_init(nk_context& ctx, nk_draw_null_texture& null_tex, const renderer::Texture& tex) {
				data = nullptr;
				width = 0;
				height = 0;

				nk_font_atlas_end(&atlas, nk_handle_id(static_cast<int>(tex.unsafe_low_level_handle())), &null_tex); // frees data
				if (atlas.default_font)
					nk_style_set_font(&ctx, &atlas.default_font->handle);
			}

			~Wnk_font_atlas() {
				nk_font_atlas_clear(&atlas);
			}
		};

		struct Nk_vertex {
			glm::vec2 position;
			glm::vec2 uv;
			nk_byte color[4];
		};

		const Vertex_layout nk_vertex_layout {
			Vertex_layout::Mode::triangles,
			vertex("position",  &Nk_vertex::position),
			vertex("uv",        &Nk_vertex::uv),
			Vertex_layout::Element{"color", 4, Vertex_layout::Element_type::ubyte_t, true, details::calcOffset(&Nk_vertex::color), 0, 0}
		};

		template<class T>
		void set_buffer(Buffer& dest, nk_buffer& src) {
			nk_memory_status info;
			nk_buffer_info(&info, &src);

			auto vbo_begin = static_cast<const T*>(nk_buffer_memory_const(&src));
			auto vbo_end = reinterpret_cast<const T*>(static_cast<const uint8_t*>(nk_buffer_memory_const(&src))+info.allocated);
			dest.set(vbo_begin, vbo_end);
		}

		struct Nk_renderer {
			Graphics_ctx& graphics_ctx;
			Shader_program prog;
			Object obj;

			Wnk_Buffer commands;
			nk_draw_null_texture null_tex;
			Wnk_Buffer vbo;
			Wnk_Buffer ibo;


			Nk_renderer(Engine& e)
			    : graphics_ctx(e.graphics_ctx()),
			      obj(nk_vertex_layout, create_dynamic_buffer<Nk_vertex>(4048),
			                            create_dynamic_buffer<nk_draw_index>(4048, true)) {

				prog.attach_shader(e.assets().load<Shader>("vert_shader:ui"_aid))
				    .attach_shader(e.assets().load<Shader>("frag_shader:ui"_aid))
				    .bind_all_attribute_locations(nk_vertex_layout)
				    .build()
				    .uniforms(make_uniform_map(
				        "texture", int(Texture_unit::temporary)
				    ));
			}
			void draw(nk_context& ctx, Camera_2d& camera) {
				glEnable(GL_SCISSOR_TEST);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_DEPTH_TEST);
				glActiveTexture(GL_TEXTURE0);

				ON_EXIT {
					glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					glDisable(GL_SCISSOR_TEST);
					glEnable(GL_DEPTH_TEST);
				};

				graphics_ctx.reset_viewport();
				auto width  = graphics_ctx.viewport().z;
				auto height = graphics_ctx.viewport().w;
				glm::vec2 scale = glm::vec2{width, height} / camera.size();

				prog.bind();
				prog.set_uniform("vp", camera.vp());

				// flush nk stuff to buffers
				nk_convert_config config;
				memset(&config, 0, sizeof(config));
				config.global_alpha = 1.0f;
				config.shape_AA = NK_ANTI_ALIASING_ON;
				config.line_AA = NK_ANTI_ALIASING_ON;
				config.circle_segment_count = 22;
				config.curve_segment_count = 22;
				config.arc_segment_count = 22;
				config.null = null_tex;

				constexpr struct nk_draw_vertex_layout_element vertex_layout[] = {
					{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(Nk_vertex, position)},
					{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(Nk_vertex, uv)},
					{NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(Nk_vertex, color)},
					{NK_VERTEX_LAYOUT_END}
				};
				config.vertex_layout = vertex_layout;
				config.vertex_size = sizeof(struct Nk_vertex);
				config.vertex_alignment = NK_ALIGNOF(struct Nk_vertex);

				nk_buffer_clear(&vbo.buffer);
				nk_buffer_clear(&ibo.buffer);
				nk_buffer_clear(&commands.buffer);
				nk_convert(&ctx, &commands.buffer, &vbo.buffer, &ibo.buffer, &config);

				set_buffer<Nk_vertex>(obj.buffer(0), vbo.buffer);
				set_buffer<nk_draw_index>(obj.index_buffer().get_or_throw(), ibo.buffer);


				// draw stuff
				auto cmd = static_cast<const nk_draw_command*>(nullptr);
				int offset = 0;
				nk_draw_foreach(cmd, &ctx, &commands.buffer) {
					if (cmd->elem_count==0) continue;
					glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(cmd->texture.id));
					glScissor(
						static_cast<GLint>(cmd->clip_rect.x * scale.x),
						static_cast<GLint>((camera.size().y - static_cast<GLint>(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
						static_cast<GLint>(cmd->clip_rect.w * scale.x),
						static_cast<GLint>(cmd->clip_rect.h * scale.y)
					);

					obj.draw(offset, static_cast<int>(cmd->elem_count));
					offset += cmd->elem_count;
				}

				nk_clear(&ctx);
			}
		};
	}

	struct Gui::PImpl {
		Camera_2d camera;
		Wnk_Context ctx;
		Wnk_Buffer buffer;
		Nk_renderer renderer;
		Wnk_font_atlas atlas;
		Texture atlas_tex;
		Gui_event_filter input_filter;
		// TODO: style/ theme

		PImpl(Engine& e)
		    : camera(e.graphics_ctx().viewport(), calculate_vscreen(e, 1000)),
		      renderer(e),
		      atlas(e),
		      atlas_tex(atlas.width, atlas.height, atlas.data, Texture_format::RGBA),
		      input_filter(e, &ctx.ctx, camera) {

			camera.position(camera.size()/2.f);

			atlas.post_init(ctx.ctx, renderer.null_tex, atlas_tex);
		}
	};

	Gui::Gui(Engine& engine)
	    : _impl(std::make_unique<PImpl>(engine)) {
	}
	Gui::~Gui() = default;

	void Gui::draw() {
		_impl->renderer.draw(_impl->ctx.ctx, _impl->camera);
	}
	auto Gui::ctx() -> nk_context* {
		return &_impl->ctx.ctx;
	}

	auto Gui::centered(int width, int height) -> struct nk_rect {
		return nk_rect(_impl->camera.size().x/2.f - width/2.f,
		               _impl->camera.size().y/2.f - height/2.f,
		               width,
		               height);
	}

}
}
