cmake_minimum_required(VERSION 3.16)

project(bbruntime)

# Portable bbruntime sources: compile on macOS without Windows headers.
set(BB_CORE_SRCS bbbank.cpp bbstream.cpp bbfilesystem.cpp bbtimer.cpp bbgraphics.cpp basic.cpp bbmath.cpp bbstring.cpp bberr.cpp bbhost.cpp)

set(SRCS
	basic.h
	${BB_CORE_SRCS}
	bbbank.h
	bbfilesystem.h
	bbmath.h
	bbruntime.cpp
	bbruntime.h
	bbsockets.cpp
	bbsockets.h
	bbstream.h
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

	# Unit tests for the debug env id table (compiler/debugenv.h, non-Windows only).
	add_executable(test_debugenv ../tests/test_debugenv.cpp ../compiler/debugenv.cpp ../compiler/environ.cpp ../compiler/type.cpp ../compiler/decl.cpp)
	bb_target_options(test_debugenv)
	target_link_libraries(test_debugenv stdutil)

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

	# x86-64 module + loader (mmap/mprotect, relocations) and the codegen export:
	# promoted from the test harness (Prompt 4 step 3).
	add_library(x64link ../linker/x64module.cpp ../linker/x64module.h ../linker/x64export.cpp ../linker/x64export.h ../linker/x64exe.cpp ../linker/x64exe.h)
	bb_target_options(x64link)
	target_link_libraries(x64link codegen_x64)

	# blitzcc for macOS: compile a .bb and run it in-process (Prompt 4 step 3).
	add_executable(blitzcc ../blitz/main_mac.cpp)
	bb_target_options(blitzcc)
	target_link_libraries(blitzcc rtlink compiler x64link bbruntime_core)
	# blitzrun: the launcher `blitzcc -o` copies in front of a compiled program.
	add_executable(blitzrun ../blitz/blitzrun_mac.cpp)
	bb_target_options(blitzrun)
	target_link_libraries(blitzrun x64link bbruntime_core)
	add_dependencies(blitzcc blitzrun)
	# `cmake --build build --target check_blitzcc`: run tests/blitzcc/*.bb through blitzcc.
	add_custom_target(check_blitzcc
		COMMAND ${CMAKE_SOURCE_DIR}/tests/blitzcc/run.sh $<TARGET_FILE:blitzcc>
		DEPENDS blitzcc VERBATIM)

	# IR dump tool: drives the real compiler front end into DumpCodegen
	# (tests/x64/), used to verify x64 backend output against the x86-32 one.
	add_executable(bbdump ../tests/x64/bbdump.cpp ../tests/x64/dump_codegen.cpp)
	bb_target_options(bbdump)
	target_link_libraries(bbdump compiler)

	# x64 backend test: emit mov eax,42; ret through the harness, run it, get 42.
	add_executable(test_codegen_x64 ../tests/x64/test_codegen_x64.cpp)
	bb_target_options(test_codegen_x64)
	target_link_libraries(test_codegen_x64 x64link)

	# x64 integer tiles (commit 5): hand-built TNode trees through Codegen_x64.
	add_executable(test_tiles_x64 ../tests/x64/test_tiles_x64.cpp)
	bb_target_options(test_tiles_x64)
	target_link_libraries(test_tiles_x64 x64link)

	# x64 calls and frames (commit 6): Blitz and System V calls, alignment, Gosub.
	add_executable(test_calls_x64 ../tests/x64/test_calls_x64.cpp)
	bb_target_options(test_calls_x64)
	target_link_libraries(test_calls_x64 x64link)

	# x64 floats (commit 7): SSE tiles, float calls and returns.
	add_executable(test_floats_x64 ../tests/x64/test_floats_x64.cpp)
	bb_target_options(test_floats_x64)
	target_link_libraries(test_floats_x64 x64link)

	# x64 data (commit 8): data section, type descriptors, data labels as symbols.
	add_executable(test_data_x64 ../tests/x64/test_data_x64.cpp)
	bb_target_options(test_data_x64)
	target_link_libraries(test_data_x64 x64link)

	# x64 front-end integration (commit 9): real .bb source through the real
	# front end into Codegen_x64, run against the stub runtime in tests/x64/rtstubs.h.
	add_executable(test_frontend_x64 ../tests/x64/test_frontend_x64.cpp)
	bb_target_options(test_frontend_x64)
	target_link_libraries(test_frontend_x64 compiler x64link)

	# x64: compiled programs against the real runtime (basic/bbmath/bbstring) through
	# the portable runtime symbol declaration (compiler/rtlink.cpp). RTEX longjmps.
	add_executable(test_realrt_x64 ../tests/x64/test_realrt_x64.cpp)
	bb_target_options(test_realrt_x64)
	target_link_libraries(test_realrt_x64 rtlink compiler x64link bbruntime_core)

	# In the portable build, only the handle-table core is available.
	# The full bbruntime library needs Windows headers (gxruntime, etc.)
	# which are not yet ported.
	return()
endif ()

add_library(bbruntime ${SRCS} bb_handle.cpp)

bb_target_options(bbruntime)

target_link_libraries(bbruntime ${LIBS})
