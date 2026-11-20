cmake_minimum_required(VERSION 3.16)

project(compiler)

#asmjit: x86-64 code generation library (portable build only, commit 4).
if(BB_PORTABLE_BUILD)
	include(FetchContent)
	FetchContent_Declare(asmjit
			GIT_REPOSITORY https://github.com/asmjit/asmjit.git
			GIT_TAG 677a10bac2c24f7e068cf1f9e675bf51c7b534ea  # pinned 2026-11-19
			GIT_SHALLOW FALSE
			EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(asmjit)
endif()

add_library(compiler
		assem_x86/asm_insts.cpp
		assem_x86/assem_x86.cpp
		assem_x86/operand.cpp
		codegen_x86/codegen_x86.cpp
		codegen_x86/tile.cpp
		debugenv.cpp
		decl.cpp
		declnode.cpp
		environ.cpp
		exprnode.cpp
		node.cpp
		parser.cpp
		prognode.cpp
		std.cpp
		stmtnode.cpp
		toker.cpp
		type.cpp
		varnode.cpp
		assem.h
		assem_x86/assem_x86.h
		assem_x86/insts.h
		assem_x86/operand.h
		codegen.h
		codegen_x86/codegen_x86.h
		codegen_x86/tile.h
		debugenv.h
		decl.h
		declnode.h
		environ.h
		ex.h
		exprnode.h
		label.h
		node.h
		nodes.h
		parser.h
		prognode.h
		std.h
		stmtnode.h
		toker.h
		type.h
		varnode.h
)

bb_target_options(compiler)

target_link_libraries(compiler stdutil)

#x64 backend (portable build only, commit 4).
if(BB_PORTABLE_BUILD)
	add_library(codegen_x64
			codegen_x64/codegen_x64.cpp
			codegen_x64/codegen_x64.h
	)
	bb_target_options(codegen_x64)
	target_link_libraries(codegen_x64 asmjit::asmjit)

	#Portable runtime symbol declaration (Prompt 4): what blitz/libs.cpp does on Windows.
	add_library(rtlink rtlink.cpp rtlink.h)
	bb_target_options(rtlink)
	target_link_libraries(rtlink compiler)
endif()
