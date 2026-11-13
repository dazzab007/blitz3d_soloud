cmake_minimum_required(VERSION 3.16)

project(stdutil)

add_library(stdutil
		stdutil.cpp
		stdutil.h
)

bb_target_options(stdutil)

