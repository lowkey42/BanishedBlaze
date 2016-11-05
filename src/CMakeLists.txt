cmake_minimum_required(VERSION 2.6)

# include core
add_subdirectory(core)

project(banished_blaze)

set(VERSION_NAME "0.1")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -std=c++1z -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field")

option(DO_LTO "Enables LTO" OFF)
if(DO_LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -std=c++14 -flto -Wextra -Wall -pedantic -Werror -Wno-unused-parameter")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

include_directories(${ROOT_DIR}/src)


if(EMSCRIPTEN)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_SDL=2 -s TOTAL_MEMORY=512000000 -s ALLOW_MEMORY_GROWTH=1 -Oz -s DEMANGLE_SUPPORT=1")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -s USE_SDL=2 -s TOTAL_MEMORY=512000000 -s ALLOW_MEMORY_GROWTH=1 -s DEMANGLE_SUPPORT=1")
	set(ZLIB_LIBRARY "")
	set(GLEW_LIBRARIES "")
	set(OPENGL_LIBRARIES "")
	set(SDL2_LIBRARY "")
	add_definitions(-DSLOW_SYSTEM)
	
elseif(ANDROID)
#	find_package(SDL2 REQUIRED)
#	include_directories(${SDL2_INCLUDE_DIR})
#	find_package(SDL2_MIXER REQUIRED)
	
	set(SDL2_LIBRARY "-lsdl2main -lSDL2")
	set(SDLMIXER_LIBRARY "-lSDL2_mixer")
	
	find_package(ZLIB)
	include_directories(${ZLIB_INCLUDE_DIR})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
	set(OPENGL_LIBRARIES "")
	set(GLEW_LIBRARIES "")
	add_definitions(-DSLOW_SYSTEM)
	
else()
	if(NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE "Debug")
	endif()

	option(SAN "Build with sanitizers" OFF)

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		MESSAGE("Debug build; Compiler=${CMAKE_CXX_COMPILER_ID}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

		if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND SAN)
			MESSAGE("Building with sanitizers")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=integer -fsanitize=undefined -fsanitize-address-use-after-scope ")

		elseif(WIN32)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")

		endif()

	else()
		if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -gline-tables-only")
		else()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
		endif()
	endif()
	
	add_definitions(-DSTACKTRACE)
	find_package(GLEW REQUIRED)

	find_package(OpenGL REQUIRED)
	include_directories(${OpenGL_INCLUDE_DIRS})
	link_directories(${OpenGL_LIBRARY_DIRS})
	add_definitions(${OpenGL_DEFINITIONS})
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIR})
	find_package(SDL2_MIXER REQUIRED)
	
	
	if(WIN32)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lmingw32 -limagehlp -lSDL2main -lSDL2_mixer -lglew32 -lws2_32 -lwsock32") # why???
		add_definitions(-DWIN)

	else()
		FIND_PACKAGE(ZLIB)
		INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-pie")
	endif()
endif()




if(EMSCRIPTEN)
	file(COPY ${ROOT_DIR}/assets/archives.lst DESTINATION "./")
	file(COPY ${ROOT_DIR}/assets/assets DESTINATION "./")
	file(COPY ${ROOT_DIR}/assets/assets_ext DESTINATION "./")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file archives.lst --preload-file assets --preload-file assets_ext -o BanishedBlaze.html")
else()
#	file(COPY ${ROOT_DIR}/assets/archives.lst DESTINATION ${BIN_DIR})
#	file(COPY ${ROOT_DIR}/assets/assets DESTINATION ${BIN_DIR})
endif(EMSCRIPTEN)


if(NOT GIT_EXECUTABLE)
    set(GIT_EXECUTABLE "git")
endif()

# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(COMMAND
  "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_HASH
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_DATE
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the subject of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%s
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SUBJECT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# generate version.cc
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/info.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/info.cpp" @ONLY)


file(GLOB_RECURSE MAGNUM_SRCS
         RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 game/*.cpp
		 game/*.h*)

add_executable(BanishedBlaze main.cpp ${MAGNUM_SRCS} "${CMAKE_CURRENT_BINARY_DIR}/info.cpp")

if(EMSCRIPTEN)
	target_link_libraries(BanishedBlaze ${SDL2_LIBRARY} core)
else()
	target_link_libraries(BanishedBlaze ${SDL2_LIBRARY} core happyhttp)
endif()


option(BUILD_TESTS "Build tests" OFF)

