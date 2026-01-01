cmake_minimum_required(VERSION 3.5)

project(QPAKMAN CXX)

include_directories(${ZLIB_INCLUDE_DIRS})

if(UNIX)
	add_definitions(-DUNIX)
elseif(WIN32)
	add_definitions(-DWIN32)
else()
	message(SEND_ERROR "You are building on unsupported platform")
endif()

add_compile_options("-O2")

add_executable(qpakman
	arc_spec.cc u_assert.cc u_file.cc u_util.cc im_color.cc im_image.cc
	 im_gen.cc im_mip.cc im_format.cc im_tex.cc archive.cc pakfile.cc main.cc)


if(WIN32 AND MSVC)
	# include setargv.obj for file wildcard expansion
	# https://learn.microsoft.com/en-us/cpp/c-language/expanding-wildcard-arguments
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} setargv.obj")
endif()


set_property(TARGET qpakman PROPERTY COMPILE_FLAGS -fno-rtti)

target_link_libraries(qpakman -lm)

install(TARGETS qpakman RUNTIME DESTINATION bin)
