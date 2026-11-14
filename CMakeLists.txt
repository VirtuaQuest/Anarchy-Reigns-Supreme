cmake_minimum_required(VERSION 3.20)

project(AnarchyReignsRecomp CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# RelWithDebInfo defaults to /Ob1 (only functions marked inline are inlined), which costs the recompiled code and
# the renderer a lot; inline like Release does.
foreach (flags CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REPLACE "/Ob1" "/Ob2" ${flags} "${${flags}}")
endforeach()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "The recompiled code requires Clang (clang-cl on Windows).")
endif()

# XenonRecomp provides the recompiler, XenonUtils (XEX loading) and shared third party libraries.
set(XENONRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/XenonRecomp" CACHE PATH "Path to XenonRecomp: the copy in this project, or a checkout to work on")
add_subdirectory(${XENONRECOMP_ROOT} XenonRecomp EXCLUDE_FROM_ALL)

add_library(o1heap STATIC "thirdparty/o1heap/o1heap.c")
target_include_directories(o1heap PUBLIC "thirdparty/o1heap")
# o1heap counts leading zeros of size_t with __builtin_clzl by default, which is 32 bit on Windows.
target_compile_definitions(o1heap PRIVATE O1HEAP_CLZ=__builtin_clzll)

add_subdirectory(thirdparty/xenia)
# The game's code comes from the player's copy of the game, by the recompiler above: before that exists, only the tools can be built (Setup.bat does this).
file(GLOB AR_GENERATED_CODE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ARRecompLib/ppc/*.cpp")
if (AR_GENERATED_CODE)
    # SDL reads controllers of every kind (ARRecomp/kernel/gamepads.cpp). SDL 3.4.20 (zlib license, thirdparty/SDL/LICENSE.txt):
    # the release's CMakeLists.txt, cmake, include and src, unchanged. Only its controller parts are built, into the game.
    foreach (option SDL_SHARED SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL SDL_AUDIO SDL_VIDEO SDL_GPU SDL_RENDER SDL_CAMERA SDL_POWER SDL_DIALOG SDL_TRAY)
        set(${option} OFF CACHE BOOL "" FORCE)
    endforeach()
    foreach (option SDL_STATIC SDL_JOYSTICK SDL_HAPTIC SDL_HIDAPI SDL_SENSOR)
        set(${option} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL)

    add_subdirectory(ARRecompLib)
    add_subdirectory(ARRecomp)
else()
    message(STATUS "No recompiled code yet: only the tools can be built.")
endif()
add_subdirectory(tools/shader_tool)
add_subdirectory(tools/launcher)
