#include "engine.hpp"

#include "asset/asset_manager.hpp"
#include "audio/audio_ctx.hpp"
#include "audio/sound.hpp"
#include "gui/translations.hpp"
#include "gui/gui.hpp"
#include "input/input_manager.hpp"
#include "graphic/graphics_ctx.hpp"
#include "utils/log.hpp"
#include "utils/rest.hpp"

#include <stdexcept>
#include <chrono>


namespace lux {
namespace {
	void init_sub_system(Uint32 f, const std::string& name, bool required=true) {
		if(SDL_InitSubSystem(f)!=0) {
			auto m = "Could not initialize "+name+": "+get_sdl_error();

			if(required)
				FAIL(m);
			else
				WARN(m);
		}
	}
}

	using namespace unit_literals;

	Sdl_event_filter::Sdl_event_filter(Engine& e) :_engine(e) {
		_engine.add_event_filter(*this);
	}

	Sdl_event_filter::~Sdl_event_filter() {
		_engine.remove_event_filter(*this);
	}

	auto get_sdl_error() -> std::string {
		std::string sdl_error(SDL_GetError());
		SDL_ClearError();
		return sdl_error;
	}

	Engine::Sdl_wrapper::Sdl_wrapper() {
		INVARIANT(SDL_Init(0)==0, "Could not initialize SDL: "<<get_sdl_error());

		init_sub_system(SDL_INIT_AUDIO, "SDL_Audio");
		init_sub_system(SDL_INIT_VIDEO, "SDL_Video");
		init_sub_system(SDL_INIT_JOYSTICK, "SDL_Joystick");
		init_sub_system(SDL_INIT_HAPTIC, "SDL_Haptic", false);
		init_sub_system(SDL_INIT_GAMECONTROLLER, "SDL_Gamecontroller");
		init_sub_system(SDL_INIT_EVENTS, "SDL_Events");
	}
	Engine::Sdl_wrapper::~Sdl_wrapper() {
		SDL_Quit();
	}


	Engine::Engine(const std::string& title, int argc, char** argv, char** env)
	  : _screens(*this),
	    _asset_manager(std::make_unique<asset::Asset_manager>(argc>0 ? argv[0] : "", title)),
	    _translator(std::make_unique<gui::Translator>(*_asset_manager)),
	    _sdl(),
	    _graphics_ctx(std::make_unique<graphic::Graphics_ctx>(title, *_asset_manager)),
	    _audio_ctx(std::make_unique<audio::Audio_ctx>(*_asset_manager)),
	    _input_manager(std::make_unique<input::Input_manager>(_bus, *_asset_manager)),
	    _gui(std::make_unique<gui::Gui>(*this)),
	    _current_time(SDL_GetTicks() / 1000.0f) {

		_input_manager->viewport(_graphics_ctx->viewport());
	}

	Engine::~Engine() noexcept {
		_screens.clear();

		if(_audio_ctx) {
			_audio_ctx->stop_music();
			_audio_ctx->pause_sounds();
		}

		assets().shrink_to_fit();
	}

	void Engine::add_event_filter(Sdl_event_filter& f) {
		_event_filter.push_back(&f);
	}
	void Engine::remove_event_filter(Sdl_event_filter& f) {
		util::erase_fast(_event_filter, &f);
	}

	namespace {
		double current_time_sec() {
#ifdef SDL_FRAMETIME
			return static_cast<float>(SDL_GetTicks()) / 1000.f;
#else
			using namespace std::chrono;
			static const auto start_time = high_resolution_clock::now();
			return duration_cast<duration<double, std::micro>>(high_resolution_clock::now()-start_time).count() / 1000.0 / 1000.0;
#endif
		}
		float smooth(float curr) {
			static float history[11] {};
			static auto i = 0;
			static auto first = true;
			if(first) {
				first = false;
				std::fill(std::begin(history), std::end(history), 1.f/60);
			}

			auto sum = 0.f;
			auto min=999.f, max=0.f;
			for(auto v : history) {
				if(v<min) {
					if(min<999.f) {
						sum += min;
					}
					min = v;
				} else if(v>max) {
					sum += max;
					max = v;
				} else {
					sum += v;
				}
			}
			curr = glm::mix(sum/9.f, curr, 0.2f);

			i = (i+1) % 11;
			history[i] = curr;
			return curr;
		}
	}

	void Engine::on_frame() {
		_last_time = _current_time;
		_current_time = current_time_sec();
		auto delta_time = std::max(0.f, static_cast<float>(_current_time - _last_time));
		auto delta_time_smoothed = smooth(std::min(delta_time, 1.f/1));

		_graphics_ctx->start_frame();

		util::rest::update();
		_bus.update();

		if(_audio_ctx) {
			_audio_ctx->flip();
		}

		_input_manager->update(delta_time * second);

		_on_frame(delta_time_smoothed * second);

		_poll_events();

		_screens.on_frame(delta_time_smoothed * second);

		_gui->draw();

		_graphics_ctx->end_frame(delta_time * second);
	}

	void Engine::_poll_events() {
		for(auto& f : _event_filter) {
			f->pre_input_events();
		}

		SDL_Event event;

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT)
				_quit = true;
			else {

				auto filtered = std::find_if(_event_filter.begin(),
				                             _event_filter.end(),
				                             [&](auto f) {
					return !f->propagate(event);
				}) != _event_filter.end();

				if(!filtered) {
					_input_manager->handle_event(event);
				}
			}

			if(event.type==SDL_KEYDOWN) {
				if(event.key.keysym.sym==SDLK_F12)
					assets().reload();
			}
		}

		for(auto& f : _event_filter) {
			f->post_input_events();
		}
	}

}
