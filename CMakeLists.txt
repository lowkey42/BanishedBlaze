cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

if(ANDROID)
	SET(PHYSFS_HAVE_CDROM_SUPPORT FALSE)
endif()

if(WIN32 OR EMSCRIPTEN OR ANDROID)
	SET(PHYSFS_INTERNAL_ZLIB TRUE)
	SET(PHYSFS_ARCHIVE_7Z FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

if(ANDROID)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_DIR}/modules")

include_directories(BEFORE "${ROOT_DIR}/dependencies")
include_directories(BEFORE "${ROOT_DIR}/dependencies/Box2D/")
include_directories(BEFORE "${ROOT_DIR}/dependencies/gsl/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/sf2/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/range-v3/include")
include_directories(BEFORE SYSTEM "${ROOT_DIR}/dependencies/nuklear")
add_definitions(-DGLM_FORCE_RADIANS)
add_definitions(-DENABLE_SF2_ASSETS)
if(NOT EMSCRIPTEN AND NOT ANDROID)
	add_definitions(-DSTACKTRACE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-pie")
endif()

set(BOX2D_BUILD_EXAMPLES OFF)
set(BOX2D_BUILD_UNITTESTS OFF)
add_subdirectory(dependencies/Box2D)
add_subdirectory(dependencies/physfs)
add_subdirectory(dependencies/soil)
add_subdirectory(dependencies/glm)
add_subdirectory(dependencies/sf2)
if(NOT EMSCRIPTEN)
	add_subdirectory(dependencies/happyhttp)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)


file(GLOB_RECURSE ALL_FILES
		 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

add_custom_target(all_project_files SOURCES ${ALL_FILES})
