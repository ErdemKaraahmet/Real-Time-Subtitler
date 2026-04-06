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
set(SDL_TRAY OFF) 
set(SDL_HIDAPI OFF) 
set(SDL_RENDER_D3D12 OFF)  
set(SDL_GPU_OPENXR OFF) 
set(SDL_RENDER_VULKAN OFF) 
set(SDL_OPENGLES OFF)

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

add_executable(Real-Time-Subtitler
    src/main.c
    src/loadConfig.c
    src/windowManager.c
    src/textTexture.c
    src/whisperEngine.c
    src/audioCapture.c
)

target_include_directories(Real-Time-Subtitler PRIVATE
    src
    deps/SDL
    deps/SDL_ttf
    deps/whisper.cpp/include
    deps/miniAudio
)

target_link_libraries(Real-Time-Subtitler 
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    whisper
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