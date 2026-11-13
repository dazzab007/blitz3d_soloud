cmake_minimum_required(VERSION 3.16)

project(bblaunch)

add_executable(bblaunch WIN32
        bblaunch.cpp
        bblaunch.ico
        bblaunch.rc
        )

bb_target_options(bblaunch)

copy_exe_to_install(bblaunch Blitz3D)
