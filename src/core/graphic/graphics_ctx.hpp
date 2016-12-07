/** Window & OpenGL-Context creation + management ****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <memory>
#include <string>
#include <SDL2/SDL.h>
#include <glm/vec3.hpp>

#include "../units.hpp"


namespace lux {
	namespace asset{
		class Asset_manager;
	}

namespace graphic {

	struct Graphics_settings {
		int width;
		int height;
		int display;
		bool fullscreen;
		bool borderless_fullscreen;
		float gamma = 2.2f;
		bool bloom = true;
		float supersampling = 1.0f;
		float shadow_softness = 0.5f;
		bool fast_lighting = false;
	};

	extern auto default_settings(int display=0) -> Graphics_settings;

	class Graphics_ctx {
		public:
			Graphics_ctx(const std::string& name, asset::Asset_manager& assets);
			~Graphics_ctx();

			void start_frame();
			void end_frame(Time delta_time);
			void set_clear_color(float r, float g, float b);

			void reset_viewport()const noexcept;

			auto win_width()const noexcept{return _win_width;}
			auto win_height()const noexcept{return _win_height;}
			auto viewport()const noexcept {return _viewport;}

			auto settings()const noexcept -> const Graphics_settings& {return *_settings;}
			bool settings(Graphics_settings);

		private:
			asset::Asset_manager& _assets;
			std::string _name;
			int _win_width, _win_height;
			glm::vec4 _viewport;
			std::shared_ptr<const Graphics_settings> _settings;

			std::unique_ptr<SDL_Window,void(*)(SDL_Window*)> _window;
			SDL_GLContext _gl_ctx;
			glm::vec3 _clear_color;
			bool _clear_color_dirty = true;

			float _frame_start_time = 0;
			float _delta_time_smoothed = 0;
			float _cpu_delta_time_smoothed = 0;
			float _time_since_last_FPS_output = 0;
	};

	struct Disable_depthtest {
		Disable_depthtest();
		~Disable_depthtest();
	};
	struct Disable_depthwrite {
		Disable_depthwrite();
		~Disable_depthwrite();
	};
	struct Disable_blend {
		Disable_blend();
		~Disable_blend();
	};
	struct Blend_add {
		Blend_add();
		~Blend_add();
	};
}
}

