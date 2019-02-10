cmake_minimum_required(VERSION 3.13)

project(clockperf C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build.h
	COMMAND tools/build.pl ${CMAKE_CURRENT_BINARY_DIR}/build.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/license.h
	COMMAND tools/license.pl ${CMAKE_CURRENT_SOURCE_DIR}/COPYING ${CMAKE_CURRENT_BINARY_DIR}/license.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/COPYING
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(clockperf clock.c drift.c main.c version.c build.h license.h)
if(NOT MSVC)
	target_link_libraries(clockperf m)
endif()
target_include_directories(clockperf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# vim: set ts=4 sts=4 sw=4 noet:
