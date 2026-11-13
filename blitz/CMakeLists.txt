cmake_minimum_required(VERSION 3.16)

project(blitz)

add_executable(blitz
		libs.cpp
		libs.h
		main.cpp
)

bb_target_options(blitz)

target_link_libraries(blitz compiler)

copy_exe_to_install(blitz bin/blitzcc)
