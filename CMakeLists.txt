cmake_minimum_required(VERSION 3.20)

if (DEFINED TARGET)
	set(CMAKE_AR zig)
	set(CMAKE_RANLIB zig)
	set(CMAKE_C_COMPILER zig cc)
	set(CMAKE_C_COMPILER_TARGET ${TARGET})
	set(CMAKE_CXX_COMPILER zig c++)
	set(CMAKE_CXX_COMPILER_TARGET ${TARGET})
	set(CMAKE_SYSTEM_NAME ${SYSTEM})
endif()

add_compile_options(-Wall -Wextra -Wpedantic -fno-exceptions)
if (NOT DEFINED TARGET)
	add_compile_options(-stdlib=libc++)
endif()

add_link_options(-stdlib=libc++)

project(memory_arena_handler VERSION 0.1 LANGUAGES CXX)

include_directories(${PROJECT_SOURCE_DIR}/src)

if (DEFINED TARGET)
	set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> ar qc <TARGET> <OBJECTS>")
	set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> ranlib <TARGET>")
endif()

option(ENABLE_CODE_COVERAGE "Enable code coverage instrumentation" OFF)

if (ENABLE_CODE_COVERAGE)
	add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
	add_link_options(-fprofile-instr-generate -u__llvm_profile_runtime)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
	googletest
	GIT_REPOSITORY ${CMAKE_SOURCE_DIR}/third_party/googletest
	GIT_TAG origin/main
)

FetchContent_MakeAvailable(
	googletest
)

enable_testing()

add_subdirectory(src)

add_library(memory_arena_handler_static STATIC
	"src/memory_arena_handler.cpp"
)

add_library(memory_arena_handler_shared SHARED
	"src/memory_arena_handler.cpp"
)
