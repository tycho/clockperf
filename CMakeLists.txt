cmake_minimum_required(VERSION 3.28)

project(clockperf C)

find_package(OpenMP)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CheckCCompilerFlag)

# Append -Wno-<name> to the list variable <out> if the compiler knows about the
# warning -W<name>. The positive form is what gets probed, because compilers
# commonly accept -Wno-whatever for warnings they have never heard of (GCC only
# mentions such a flag if some other diagnostic happens to fire later on).
function(check_disable_warning _out _name)
	string(MAKE_C_IDENTIFIER "HAVE_W_${_name}" _have_var)
	# Some compilers merely warn about flags they don't understand, so -Werror
	# is needed to turn that diagnostic into a hard failure.
	set(CMAKE_REQUIRED_FLAGS -Werror)
	check_c_compiler_flag(-W${_name} ${_have_var})
	if(${_have_var})
		set(_flags ${${_out}})
		list(APPEND _flags -Wno-${_name})
		set(${_out} ${_flags} PARENT_SCOPE)
	endif()
endfunction()

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

add_executable(clockperf affinity.c clock.c drift.c main.c util.c version.c ${GETOPT_SOURCES} build.h license.h)
target_link_libraries(clockperf Threads::Threads)
if (OpenMP_FOUND)
	target_link_libraries(clockperf OpenMP::OpenMP_C)
endif()
if(NOT MSVC)
	target_link_libraries(clockperf m)
	set(WARNING_FLAGS -Wno-deprecated-declarations)
	check_disable_warning(WARNING_FLAGS deprecated-openmp)
	target_compile_options(clockperf PRIVATE ${WARNING_FLAGS})
endif()
if(WIN32 OR CYGWIN OR MINGW)
	target_link_libraries(clockperf winmm)
endif()
target_include_directories(clockperf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${GETOPT_INCLUDE_DIR})

# vim: set ts=4 sts=4 sw=4 noet:
