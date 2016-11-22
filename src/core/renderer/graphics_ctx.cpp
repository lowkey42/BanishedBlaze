#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#ifdef EMSCRIPTEN
	#include <html5.h>
#endif

#include "graphics_ctx.hpp"

#include "debug_renderer.hpp"
#include "text.hpp"
#include "sprite_batch.hpp"
#include "texture_batch.hpp"
#include "material.hpp"
#include "primitives.hpp"

#include "../utils/log.hpp"
#include "../asset/asset_manager.hpp"

#include <sf2/sf2.hpp>

#include <iostream>
#include <sstream>
#include <cstdio>


namespace lux {
namespace renderer {

	using namespace unit_literals;

	namespace {
		void sdl_error_check() {
			const char *err = SDL_GetError();
			if(*err != '\0') {
				std::string errorStr(err);
				SDL_ClearError();
				FAIL("SDL: "<<errorStr);
			}
		}

		void enable_extension(const char* name) {
#ifdef EMSCRIPTEN
			auto ctx = emscripten_webgl_get_current_context();
			INVARIANT(ctx!=0, "No active WebGL context.");

			auto result = emscripten_webgl_enable_extension(ctx, name);
			INVARIANT(result, "Required extension '"<<name<<"' is not supported by your browser.");
#else
			(void) name;
#endif
		}

		void enable_required_extensions() {
#ifdef EMSCRIPTEN
			enable_extension("EXT_color_buffer_float");
			enable_extension("EXT_texture_filter_anisotropic");
			enable_extension("OES_texture_float_linear");
#else
			(void) enable_extension;
#endif
		}

#ifndef ANDROID
	#ifndef EMSCRIPTEN
		void
	#ifdef GLAPIENTRY
		GLAPIENTRY
	#endif
		gl_debug_callback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar* message,const void*) {
			if(severity!=GL_DEBUG_SEVERITY_LOW
			   && severity!=GL_DEBUG_SEVERITY_MEDIUM
			   && severity!=GL_DEBUG_SEVERITY_HIGH)
				return;

			auto& log = [&]() -> auto& {
				switch (severity){
					case GL_DEBUG_SEVERITY_LOW:
					case GL_DEBUG_SEVERITY_MEDIUM:
						return util::warn("", "", 0);

					case GL_DEBUG_SEVERITY_HIGH:
						return util::error("", "", 0);
					default:
						return util::info("", "", 0);
				}
			}();
			log<<"[GL] ";
			// TODO: source


			switch (type) {
				case GL_DEBUG_TYPE_ERROR:
					log << "ERROR";
					break;
				case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
					log << "DEPRECATED_BEHAVIOR";
					break;
				case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
					log << "UNDEFINED_BEHAVIOR";
					break;
				case GL_DEBUG_TYPE_PORTABILITY:
					log << "PORTABILITY";
					break;
				case GL_DEBUG_TYPE_PERFORMANCE:
					log << "PERFORMANCE";
					break;
				case GL_DEBUG_TYPE_OTHER:
					log << "OTHER";
					break;
			}

			log<<"("<<id<<"): "<<message;

			log<<std::endl;
		}
	#endif
#endif
	}

	sf2_structDef(Graphics_settings,
		width,
		height,
		display,
		fullscreen,
		borderless_fullscreen,
		gamma,
		bloom,
		supersampling,
		shadow_softness,
		fast_lighting
	)

	auto default_settings(int display) -> Graphics_settings {
#ifndef EMSCRIPTEN
		SDL_DisplayMode native_mode;
		if(SDL_GetDesktopDisplayMode(display, &native_mode)!=0) {
			INFO("Couldn't detect the native resolution => using default 1920x1080");
			native_mode.w = 1920;
			native_mode.h = 1080;
		}

		Graphics_settings s;
		s.width = native_mode.w;
		s.height = native_mode.h;
		s.display = display;
		s.fullscreen = true;
		s.borderless_fullscreen = true;
		s.gamma = 2.2f;
		s.bloom = true;
		s.supersampling = 1.f;
		s.shadow_softness = 0.5f;
		s.fast_lighting = false;

		return s;

#else
		Graphics_settings s;
		s.width = 2048;
		s.height = 1024;
		s.display = 0;
		s.fullscreen = false;
		s.borderless_fullscreen = false;
		s.gamma = 2.2f;
		s.bloom = false;
		s.supersampling = 0.5f;
		s.shadow_softness = 0.0f;
		s.fast_lighting = false;

		return s;
#endif
	}

	Graphics_ctx::Graphics_ctx(const std::string& name, asset::Asset_manager& assets)
	 : _assets(assets), _name(name), _window(nullptr, SDL_DestroyWindow) {

		auto maybe_settings = assets.load_maybe<Graphics_settings>("cfg:graphics"_aid);
		if(maybe_settings.is_nothing()) {
			if(!settings(default_settings())) {
				FAIL("Invalid graphics settings");
			}
		} else {
			_settings = maybe_settings.get_or_throw();
		}


#ifdef EMSCRIPTEN
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

#ifdef _NDEBUG
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		auto win_flags = Uint32(SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

		auto display = static_cast<unsigned int>(_settings->display);
#ifndef EMSCRIPTEN
		_window.reset(SDL_CreateWindow(_name.c_str(),
		                               int(SDL_WINDOWPOS_CENTERED_DISPLAY(display)), int(SDL_WINDOWPOS_CENTERED_DISPLAY(display)),
		                               _settings->width, _settings->height, win_flags) );

		if (!_window)
			FAIL("Unable to create window");

		sdl_error_check();

		if(!settings(*_settings)) { //< apply actual size/settings
			FAIL("Couldn't apply graphics settings");
		}
#else
		_window.reset(SDL_CreateWindow(_name.c_str(),
		                               SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
		                               _settings->width, _settings->height, win_flags) );

		if (!_window)
			FAIL("Unable to create window");

		sdl_error_check();

		_win_width = _settings->width;
		_win_height = _settings->height;
		_viewport = glm::vec4{0,0,_settings->width, _settings->height};
		reset_viewport();
#endif

		sdl_error_check();

		try {
			_gl_ctx = SDL_GL_CreateContext(_window.get());
			sdl_error_check();
			SDL_GL_MakeCurrent(_window.get(), _gl_ctx);
			sdl_error_check();

		} catch (const std::runtime_error& ex) {
			FAIL("Failure to create OpenGL context. This application requires a OpenGL ES 2.0 capable GPU. Error was: "<< ex.what());
		}

		if(SDL_GL_SetSwapInterval(-1)!=0) SDL_GL_SetSwapInterval(1);

#ifndef ANDROID
		glewExperimental = GL_TRUE;
		glewInit();

	#ifndef EMSCRIPTEN
		if(GLEW_KHR_debug){
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)gl_debug_callback, stderr);
		}
		else{
			WARN("No OpenGL debug log available.");
		}
	#endif
#endif

		enable_required_extensions();

		glLineWidth(2.0f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
		glEnable(GL_POINT_SPRITE);
		set_clear_color(0.0f,0.0f,0.0f);

		init_font_renderer(assets);
		init_sprite_renderer(assets);
		init_texture_renderer(assets);
		init_materials(assets);
		init_primitives(assets);
		init_debug_renderer(assets, *this);
	}

	Graphics_ctx::~Graphics_ctx() {
		SDL_GL_DeleteContext(_gl_ctx);
	}

	void Graphics_ctx::reset_viewport()const noexcept {
#ifndef EMSCRIPTEN
		glViewport(_viewport.x, _viewport.y, _viewport.z, _viewport.w);
#endif
	}

	void Graphics_ctx::start_frame() {
		if(_clear_color_dirty) {
			glClearColor(_clear_color.r, _clear_color.g, _clear_color.b,1.f);
			_clear_color_dirty = false;
		}

		glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

		_frame_start_time = SDL_GetTicks() / 1000.0f;
	}
	void Graphics_ctx::end_frame(Time delta_time) {
		float smooth_factor=0.1f;
		_delta_time_smoothed=(1.0f-smooth_factor)*_delta_time_smoothed+smooth_factor*delta_time/second;

		float cpu_delta_time = SDL_GetTicks() / 1000.0f - _frame_start_time;
		_cpu_delta_time_smoothed=(1.0f-smooth_factor)*_cpu_delta_time_smoothed+smooth_factor*cpu_delta_time;

		_time_since_last_FPS_output+=delta_time/second;
		if(_time_since_last_FPS_output>=1.0f){
			_time_since_last_FPS_output=0.0f;
			std::ostringstream osstr;
			osstr<<_name<<" ("<<(int((1.0f/_delta_time_smoothed)*10.0f)/10.0f)<<" FPS, ";
			osstr<<(int(_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame, ";
			osstr<<(int(_cpu_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame [cpu])";

#ifdef EMSCRIPTEN
			// DEBUG(_cpu_delta_time_smoothed);
#else
			SDL_SetWindowTitle(_window.get(), osstr.str().c_str());
#endif
		}

		on_frame_debug_draw();

		SDL_GL_SwapWindow(_window.get());
	}
	void Graphics_ctx::set_clear_color(float r, float g, float b) {
		_clear_color = glm::vec3(r,g,b);
		_clear_color_dirty = true;
	}

	bool Graphics_ctx::settings(Graphics_settings new_settings) {
		SDL_DisplayMode target, closest;
		target.w = new_settings.width;
		target.h = new_settings.height;
		target.format       = 0;
		target.refresh_rate = 0;
		target.driverdata   = 0;

		if(new_settings.fullscreen) {
			if(SDL_GetClosestDisplayMode(new_settings.display, &target, &closest) == nullptr) {
				return false;
			}

			new_settings.width = closest.w;
			new_settings.height = closest.h;
		}

		INFO("Using resolution "<<new_settings.width<<"x"<<new_settings.height);

		_assets.save<Graphics_settings>("cfg:graphics"_aid, new_settings);
		_settings = _assets.load<Graphics_settings>("cfg:graphics"_aid);

#ifndef EMSCRIPTEN
		// update existing window
		if(_window) {
			SDL_SetWindowSize(_window.get(), _settings->width, _settings->height);

			if(_settings->fullscreen) {
				auto fs_type = _settings->borderless_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
				SDL_SetWindowFullscreen(_window.get(), fs_type);
				SDL_SetWindowDisplayMode(_window.get(), &closest);
			}

			SDL_GetWindowSize(_window.get(), &_win_width, &_win_height);
			int drawable_width, drawable_height;
			SDL_GL_GetDrawableSize(_window.get(), &drawable_width, &drawable_height);
			_viewport = glm::vec4{0,0,drawable_width,drawable_height};

			reset_viewport();
		}
#endif

		return true;
	}

	Disable_depthtest::Disable_depthtest() {
		glDisable(GL_DEPTH_TEST);
	}
	Disable_depthtest::~Disable_depthtest() {
		glEnable(GL_DEPTH_TEST);
	}

	Disable_depthwrite::Disable_depthwrite() {
		glDepthMask(GL_FALSE);
	}
	Disable_depthwrite::~Disable_depthwrite() {
		glDepthMask(GL_TRUE);
	}

	Disable_blend::Disable_blend() {
		glDisable(GL_BLEND);
	}
	Disable_blend::~Disable_blend() {
		glEnable(GL_BLEND);
	}

	Blend_add::Blend_add() {
		glBlendFunc(GL_ONE, GL_ONE);
	}

	Blend_add::~Blend_add() {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}
}
}
