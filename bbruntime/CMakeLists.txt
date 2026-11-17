cmake_minimum_required(VERSION 3.16)

project(bbruntime)

# Portable bbruntime sources: compile on macOS without Windows headers.
set(BB_CORE_SRCS bbbank.cpp bbstream.cpp bbfilesystem.cpp bbtimer.cpp bbgraphics.cpp)

set(SRCS
	basic.cpp
	basic.h
	${BB_CORE_SRCS}
	bbbank.h
	bbfilesystem.h
	bbmath.cpp
	bbmath.h
	bbruntime.cpp
	bbruntime.h
	bbsockets.cpp
	bbsockets.h
	bbstream.h
	bbstring.cpp
	bbstring.h
	bbsys.cpp
	bbsys.h
	std.cpp
	std.h
	userlibs.cpp
	userlibs.h
)
set(LIBS gxruntime)

if (BB_BLITZ3D_ENABLED)

	set(SRCS ${SRCS}
        bbinput.cpp
        bbinput.h
        bbblitz3d.cpp
        bbblitz3d.h
        bbgraphics.cpp
        bbgraphics.h
	)
    set(LIBS gxruntime blitz3d)

    if(BB_FMOD_ENABLED)
        set(SRCS ${SRCS}
            bbaudio.cpp
            bbaudio.h
        )
        set(LIBS ${LIBS} fmod375)
    else()
        set(SRCS ${SRCS}
           bbaudio_soloud.cpp
           bbaudio_soloud.h
        )
        set(LIBS ${LIBS} soloud)
    endif()

elseif(BB_LIBSGD_ENABLED)

	set(SRCS ${SRCS}
		bbsgd.cpp
		bbsgd.h
	)
	set(LIBS gxruntime libsgd)

endif()

if (BB_PORTABLE_BUILD)
	# Handle table core (macOS 64-bit only; no-op on _WIN32).
	add_library(bb_handle bb_handle.cpp bb_handle.h)
	bb_target_options(bb_handle)

	# Portable bbruntime: files that compile on macOS without Windows headers.
	# bbaudio_soloud.cpp is added here (not to BB_CORE_SRCS): the full Windows
	# build picks soloud vs FMOD conditionally, so it must not be unconditional.
	add_library(bbruntime_core ${BB_CORE_SRCS} bb_handle.cpp bbaudio_soloud.cpp)
	bb_target_options(bbruntime_core)
	# stdutil: bbblitz3d.cpp (linked into test_bbblitz3d) uses itoa/filenamefile/tolower.
	target_link_libraries(bbruntime_core bb_handle soloud blitz3d stdutil)

	# 3D handle-table integration test: links real bbblitz3d.cpp + the blitz3d
	# library + stubs (gxScene/gxGraphics scene methods, etc.).
	add_executable(test_bbblitz3d ../tests/test_bbblitz3d.cpp ../tests/stubs_gxruntime.cpp bbblitz3d.cpp)
	bb_target_options(test_bbblitz3d)
	target_compile_definitions(test_bbblitz3d PRIVATE STUBS_NO_BLITZ3D_OPEN_CLOSE)
	target_link_libraries(test_bbblitz3d bbruntime_core)


	# Stream + filesystem handle-table integration test.
	add_executable(test_bbstream ../tests/test_bbstream.cpp ../tests/stubs_gxruntime.cpp)
	bb_target_options(test_bbstream)
	target_link_libraries(test_bbstream bbruntime_core)

	# Unit tests for the handle table (no graphics/audio dependencies).
	add_executable(test_handle_table ../tests/test_handle_table.cpp)
	bb_target_options(test_handle_table)
	target_link_libraries(test_handle_table bb_handle)

	# bbbank integration test: links real bbbank.cpp + stubs for gxruntime.
	add_executable(test_bbbank ../tests/test_bbbank.cpp ../tests/stubs_gxruntime.cpp)
	bb_target_options(test_bbbank)
	target_link_libraries(test_bbbank bbruntime_core)

	# Timer handle-table integration test: links real bbtimer.cpp + stubs.
	add_executable(test_bbtimer ../tests/test_bbtimer.cpp ../tests/stubs_gxruntime.cpp)
	bb_target_options(test_bbtimer)
	target_link_libraries(test_bbtimer bbruntime_core)

	# Graphics handle-table integration test: links real bbgraphics.cpp + stubs.
	add_executable(test_bbgraphics ../tests/test_bbgraphics.cpp ../tests/stubs_gxruntime.cpp)
	bb_target_options(test_bbgraphics)
	target_link_libraries(test_bbgraphics bbruntime_core)

	# Audio handle-table integration test: links real bbaudio_soloud.cpp + stubs,
	# using SoLoud's null backend so no audio device is needed.
	add_executable(test_bbaudio ../tests/test_bbaudio.cpp ../tests/stubs_gxruntime.cpp)
	bb_target_options(test_bbaudio)
	target_link_libraries(test_bbaudio bbruntime_core)

	# In the portable build, only the handle-table core is available.
	# The full bbruntime library needs Windows headers (gxruntime, etc.)
	# which are not yet ported.
	return()
endif ()

add_library(bbruntime ${SRCS} bb_handle.cpp)

bb_target_options(bbruntime)

target_link_libraries(bbruntime ${LIBS})