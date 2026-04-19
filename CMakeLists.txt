cmake_minimum_required(VERSION 3.20)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(Real-Time-Subtitler C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# SDL flags
set(SDL_JOYSTICK OFF) 
set(SDL_HAPTIC OFF) 
set(SDL_CAMERA OFF) 
set(SDL_SENSOR OFF) 
set(SDL_DIALOG OFF) 
set(SDL_TRAY ON) 
set(SDL_HIDAPI OFF) 
set(SDL_RENDER_D3D12 OFF)  
set(SDL_GPU_OPENXR OFF) 
set(SDL_RENDER_VULKAN ON) 
set(SDL_OPENGLES OFF)
set(SDL_CCACHE ON)

# Apply SDL mouse passthrough patch if not already applied
execute_process(
    COMMAND git apply --check ${CMAKE_SOURCE_DIR}/deps/patches/sdl_mouse_passthrough.patch
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/SDL
    RESULT_VARIABLE patch_check
    ERROR_QUIET
    OUTPUT_QUIET
)
if(patch_check EQUAL 0)
    message(STATUS "Applying SDL mouse passthrough patch...")
    execute_process(
        COMMAND git apply ${CMAKE_SOURCE_DIR}/deps/patches/sdl_mouse_passthrough.patch
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/SDL
    )
endif()

add_subdirectory(deps/SDL)
add_subdirectory(deps/SDL_ttf)
set(GGML_OPENMP OFF)
set(GGML_VULKAN ON)
add_subdirectory(deps/whisper.cpp)

# Build cimgui (Dear ImGui C bindings) as a static library
set(IMGUI_STATIC "yes" CACHE STRING "" FORCE)
add_subdirectory(deps/cimgui)
# Give cimgui access to SDL3 headers so imgui can use SDL types
target_include_directories(cimgui PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/SDL/include
)

add_executable(Real-Time-Subtitler
    src/main.c
    src/loadConfig.c
    src/windowManager.c
    src/textTexture.c
    src/whisperEngine.c
    src/audioCapture.c
    src/trayManager.c
    # ImGui SDL3 + SDLRenderer3 backends (not compiled by cimgui target)
    deps/cimgui/imgui/backends/imgui_impl_sdl3.cpp
    deps/cimgui/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(Real-Time-Subtitler PRIVATE
    src
    deps/SDL
    deps/SDL_ttf
    deps/whisper.cpp/include
    deps/miniAudio
    deps/cimgui
    deps/cimgui/imgui
    deps/cimgui/generator/output
    deps/cimgui/imgui/backends
)

target_compile_definitions(Real-Time-Subtitler PRIVATE
    CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    CIMGUI_USE_SDL3
    CIMGUI_USE_SDLRENDERER3
    IMGUI_USER_CONFIG="../cimconfig.h"
)

target_link_libraries(Real-Time-Subtitler 
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    whisper
    cimgui
    -static-libgcc
    -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
)
# copy SDL dll
add_custom_command(TARGET Real-Time-Subtitler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3>
    $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
    $<TARGET_FILE:whisper>
    $<TARGET_FILE:ggml>
    $<TARGET_FILE:ggml-base>
    $<TARGET_FILE:ggml-cpu>
    $<TARGET_FILE:ggml-vulkan>
    $<TARGET_FILE_DIR:Real-Time-Subtitler>
)