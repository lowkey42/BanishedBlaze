/** application's entry point ************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include "core/utils/log.hpp"
#include "core/utils/stacktrace.hpp"

#include "game/game_engine.hpp"

#include "game/editor_screen.hpp"
#include "game/main_menu_screen.hpp"
#include "game/world_map_screen.hpp"

#include "info.hpp"

#include <iostream>
#include <exception>
#include <SDL2/SDL.h>

using namespace lux; // import game namespace
using namespace std::string_literals;


namespace {
	std::unique_ptr<Game_engine> engine;

	void init_env(int argc, char** argv, char** env);
	void init_engine();
	void onFrame();
	void shutdown();

#ifdef EMSCRIPTEN
	void init_ems() {
		if(lux::asset::storage_ready()) {
			DEBUG("init");
			init_engine();

			emscripten_cancel_main_loop();
			emscripten_set_main_loop(onFrame, 0, 0);
		}
	}
#endif
}


#ifdef main
int main(int argc, char** argv) {
	char* noEnv = nullptr;
	char** env = &noEnv;
#else
int main(int argc, char** argv, char** env) {
#endif

	init_env(argc, argv, env);

	#ifdef EMSCRIPTEN
		emscripten_set_main_loop(init_ems, 0, 1);
		shutdown();
		emscripten_exit_with_live_runtime();


	#else
		init_engine();

		while(engine->running())
			onFrame();

		shutdown();
	#endif

	return 0;
}

namespace {
	constexpr auto app_name = "BanishedBlaze";
	int argc;
	char** argv;
	char** env;

	void init_env(int argc, char** argv, char** env) {
		::argc = argc;
		::argv = argv;
		::env  = env;

		INFO("Game started from: "<<argv[0]<<"\n"
		     <<"Working dir: "<<asset::pwd()<<"\n"
		     <<"Version: "<<version_info::name<<"\n"
		     <<"Version-Hash: "<<version_info::hash<<"\n"
			 <<"Version-Date: "<<version_info::date<<"\n"
			 <<"Version-Subject: "<<version_info::subject<<"\n");
		// TODO: print system information

		try {
			util::init_stacktrace(argv[0]);
			lux::asset::setup_storage();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in init: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in init", nullptr);
			shutdown();
			exit(1);
		}
	}

	void init_engine() {
		try {
			engine = std::make_unique<Game_engine>(app_name, argc, argv, env);

			if(argc>1 && argv[1]=="game"s)
				engine->screens().enter<World_map_screen>("jungle");
			else if(argc>2 && argv[1]=="editor"s)
				engine->screens().enter<Editor_screen>(argv[2]);
			else
				engine->screens().enter<Main_menu_screen>(); // TODO: intro screen ?

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in init: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in init", nullptr);
			shutdown();
			exit(1);
		}
	}

	void onFrame() {
		try {
			engine->on_frame();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in onFrame: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in onFrame", nullptr);
			shutdown();
			exit(2);
		}
	}

	void shutdown() {
		try {
			engine.reset();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in shutdown: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in shutdown", nullptr);
			exit(3);
		}
	}
}
