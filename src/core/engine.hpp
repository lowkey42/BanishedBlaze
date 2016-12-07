/** initialization, live-cycle management & glue-code ************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "screen.hpp"

#include "utils/maybe.hpp"
#include "utils/messagebus.hpp"

#include <vector>
#include <memory>


union SDL_Event;

namespace lux {
	namespace asset {class Asset_manager;}
	namespace input {class Input_manager;}
	namespace graphic {class Graphics_ctx;}
	namespace audio {class Audio_ctx;}
	namespace gui {class Translator; class Gui;}

	struct Sdl_event_filter {
		Sdl_event_filter(Engine&);
		virtual ~Sdl_event_filter();
		virtual bool propagate(SDL_Event&) = 0;
		virtual void pre_input_events() {}
		virtual void post_input_events() {}

		private:
			Engine& _engine;
	};

	extern std::string get_sdl_error();

	class Engine {
		public:
			Engine(const std::string& title, int argc, char** argv, char** env);
			virtual ~Engine() noexcept;

			bool running() const noexcept {return !_quit;}
			void exit() noexcept {_quit = true;}

			void add_event_filter(Sdl_event_filter&);
			void remove_event_filter(Sdl_event_filter&);

			void on_frame();

			auto& graphics_ctx()noexcept {return *_graphics_ctx;}
			auto& graphics_ctx()const noexcept {return *_graphics_ctx;}
			auto& audio_ctx() const noexcept {return *_audio_ctx;}
			auto& audio_ctx() noexcept {return *_audio_ctx;}
			auto& assets()noexcept {return *_asset_manager;}
			auto& assets()const noexcept {return *_asset_manager;}
			auto& input()noexcept {return *_input_manager;}
			auto& input()const noexcept {return *_input_manager;}
			auto& bus()noexcept {return _bus;}
			auto& screens()noexcept {return _screens;}
			auto& translator()noexcept {return *_translator;}
			auto& gui()noexcept {return *_gui;}

		protected:
			void _poll_events();
			virtual void _on_frame(Time) {}

		protected:
			struct Sdl_wrapper {
				Sdl_wrapper();
				~Sdl_wrapper();
			};

			bool _quit = false;
			Screen_manager _screens;
			util::Message_bus _bus;
			std::unique_ptr<asset::Asset_manager> _asset_manager;
			std::unique_ptr<gui::Translator> _translator;
			Sdl_wrapper _sdl;
			std::vector<Sdl_event_filter*> _event_filter;
			std::unique_ptr<graphic::Graphics_ctx> _graphics_ctx;
			std::unique_ptr<audio::Audio_ctx> _audio_ctx;
			std::unique_ptr<input::Input_manager> _input_manager;
			std::unique_ptr<gui::Gui> _gui;

			double _current_time = 0;
			double _last_time = 0;
	};

}
