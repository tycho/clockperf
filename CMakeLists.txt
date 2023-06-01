cmake_minimum_required(VERSION 3.6)
# At time of writing, Cygwin has 3.6, MinGW has 3.12

project(clockperf C)

find_package(OpenMP)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build.h
	COMMAND perl tools/build.pl ${CMAKE_CURRENT_BINARY_DIR}/build.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/license.h
	COMMAND perl tools/license.pl ${CMAKE_CURRENT_SOURCE_DIR}/COPYING ${CMAKE_CURRENT_BINARY_DIR}/license.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/COPYING
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
	set(GETOPT_SOURCES getopt/getopt_long.c)
	set(GETOPT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/getopt)

	# Visual Studio likes to complain about how "snprintf" should be
	# "_snprintf" or whatever, which would destroy portability for no reason.
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

add_executable(clockperf affinity.c clock.c drift.c main.c tscemu.c util.c winapi.c version.c ${GETOPT_SOURCES} build.h license.h)
target_link_libraries(clockperf Threads::Threads)
set_property(TARGET clockperf PROPERTY C_STANDARD 11)
if (OpenMP_FOUND)
	target_link_libraries(clockperf OpenMP::OpenMP_C)
endif()
if(NOT MSVC)
	target_link_libraries(clockperf m)
	target_compile_options(clockperf PRIVATE -Wno-deprecated-declarations)
endif()
if(WIN32 OR CYGWIN OR MINGW)
	target_link_libraries(clockperf winmm)
endif()
target_include_directories(clockperf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${GETOPT_INCLUDE_DIR})

# vim: set ts=4 sts=4 sw=4 noet:
