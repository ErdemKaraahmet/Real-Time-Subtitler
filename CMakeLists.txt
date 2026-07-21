cmake_minimum_required(VERSION 3.24)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(NOT RTS_VERSION)
    set(RTS_VERSION "0.0.0")
endif()
string(REGEX REPLACE "^v" "" RTS_VERSION_CLEAN "${RTS_VERSION}")

project(Real-Time-Subtitler VERSION ${RTS_VERSION_CLEAN} LANGUAGES C CXX)

configure_file(
    "${CMAKE_SOURCE_DIR}/include/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/version.h"
)

find_package(CURL REQUIRED)

if(WIN32)
    enable_language(RC)
endif()

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Enable portable RPATH so shared libs are found next to the executable, cross-platform
if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
elseif(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

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
    COMMAND git apply --check "${CMAKE_SOURCE_DIR}/deps/patches/sdl_mouse_passthrough.patch"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/deps/SDL"
    RESULT_VARIABLE patch_check
    ERROR_QUIET
    OUTPUT_QUIET
)
if(patch_check EQUAL 0)
    message(STATUS "Applying SDL mouse passthrough patch...")
    execute_process(
        COMMAND git apply "${CMAKE_SOURCE_DIR}/deps/patches/sdl_mouse_passthrough.patch"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/deps/SDL"
        RESULT_VARIABLE patch_apply_result
    )
    if(NOT patch_apply_result EQUAL 0)
        message(FATAL_ERROR "Failed to apply SDL mouse passthrough patch even though git apply --check succeeded. Aborting configuration.")
    endif()
endif()

add_subdirectory(deps/SDL)

set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored third-party libraries" FORCE)
add_subdirectory(deps/SDL_ttf)

# Disable freetype's version resource compilation to prevent duplicate resource conflicts with our resources.rc
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/deps/SDL_ttf/external/freetype/src/base/ftver.rc"
    DIRECTORY "${CMAKE_SOURCE_DIR}/deps/SDL_ttf/external/freetype"
    PROPERTIES HEADER_FILE_ONLY ON
)
set(GGML_OPENMP OFF)

option(RTS_GPU "Build with GPU acceleration" ON)

if(RTS_GPU)
    if(APPLE)
        message(STATUS "Building with Metal GPU acceleration.")
        set(GGML_METAL ON CACHE BOOL "Use Metal" FORCE)
    else()
        find_package(Vulkan COMPONENTS glslc REQUIRED)
        find_package(SPIRV-Headers REQUIRED)
        message(STATUS "Vulkan found and SPIRV-Headers found. Building with Vulkan GPU acceleration.")
        set(GGML_VULKAN ON CACHE BOOL "Use Vulkan" FORCE)
    endif()
else()
    message(STATUS "Building without GPU acceleration.")
    set(GGML_VULKAN OFF CACHE BOOL "Use Vulkan" FORCE)
    set(GGML_METAL OFF CACHE BOOL "Use Metal" FORCE)
endif()

# Force building whisper and ggml as static libraries to avoid unsigned DLL loading blocks (error 0xc0e90002) on Windows 11
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(deps/whisper.cpp)

# Build cimgui (Dear ImGui C bindings) as a static library
set(IMGUI_STATIC "yes" CACHE STRING "" FORCE)
add_subdirectory(deps/cimgui)
# Give cimgui access to SDL3 headers so imgui can use SDL types
target_include_directories(cimgui PRIVATE
    "${CMAKE_SOURCE_DIR}/deps/SDL/include"
)

set(RTS_SOURCES
    src/main.c
    src/configManager.c
    src/windowManager.c
    src/textTexture.c
    src/whisperEngine.c
    src/audioCapture.c
    src/trayManager.c
    src/controlPanel.c
    src/modelManager.c
    src/utils.c
    src/imgui_impl_sdlrenderer3_c.cpp
    deps/cJSON/cJSON.c
    deps/sha256/sha256.c
    # ImGui SDL3 + SDLRenderer3 backends (not compiled by cimgui target)
    deps/cimgui/imgui/backends/imgui_impl_sdl3.cpp
    deps/cimgui/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

if(WIN32)
    add_library(RTS_Icon OBJECT src/resources.rc)
endif()

add_executable(Real-Time-Subtitler ${RTS_SOURCES})

if(WIN32)
    target_link_libraries(Real-Time-Subtitler RTS_Icon)
endif()

target_include_directories(Real-Time-Subtitler PRIVATE
    src
    include
    "${CMAKE_BINARY_DIR}/generated"
    deps/SDL
    deps/SDL_ttf
    deps/whisper.cpp/include
    deps/miniAudio
    deps/cimgui
    deps/cimgui/imgui
    deps/cimgui/generator/output
    deps/cimgui/imgui/backends
    deps/cJSON
    deps/sha256
)

option(RTS_BENCH "Enable inference benchmark logging to rts_bench.csv" OFF)
option(RTS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(RTS_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(RTS_ENABLE_SANITIZERS AND RTS_ENABLE_TSAN)
    message(FATAL_ERROR "RTS_ENABLE_SANITIZERS (ASan/UBSan) and RTS_ENABLE_TSAN (TSan) cannot be enabled together. Please enable only one.")
endif()

if(RTS_ENABLE_TSAN AND MINGW)
    message(FATAL_ERROR "RTS_ENABLE_TSAN is not supported on MinGW GCC. ThreadSanitizer requires Linux or Clang.")
endif()

# Compiler Warning & Hardening Flags (GCC / Clang)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Real-Time-Subtitler PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wstrict-prototypes -Wformat=2>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2>
    )

    # Stack protection for non-release builds
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(Real-Time-Subtitler PRIVATE -fstack-protector-strong)
    endif()

    # Fortify source for RelWithDebInfo (requires optimization -O1+)
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_definitions(Real-Time-Subtitler PRIVATE _FORTIFY_SOURCE=2)
    endif()
endif()

if(RTS_ENABLE_SANITIZERS)
    message(STATUS "Enabling AddressSanitizer & UndefinedBehaviorSanitizer")
    target_compile_options(Real-Time-Subtitler PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(Real-Time-Subtitler PRIVATE -fsanitize=address,undefined)
elseif(RTS_ENABLE_TSAN)
    message(STATUS "Enabling ThreadSanitizer")
    target_compile_options(Real-Time-Subtitler PRIVATE -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(Real-Time-Subtitler PRIVATE -fsanitize=thread)
endif()

target_compile_definitions(Real-Time-Subtitler PRIVATE
    CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    CIMGUI_USE_SDL3
    CIMGUI_USE_SDLRENDERER3
    IMGUI_USER_CONFIG="../cimconfig.h"
    $<$<BOOL:${RTS_BENCH}>:RTS_BENCH>
)

target_link_libraries(Real-Time-Subtitler 
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    whisper
    cimgui
    CURL::libcurl
)

if(WIN32 AND MINGW)
    target_link_libraries(Real-Time-Subtitler
        -static-libgcc
        -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
        -static-libstdc++
    )
endif()

# copy shared libraries next to the exe on every platform (whisper/ggml are statically linked)
if(WIN32)
    set(RTS_COPY_LIBS
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
    )
else()
    set(RTS_COPY_LIBS
        $<TARGET_SONAME_FILE:SDL3::SDL3>
        $<TARGET_SONAME_FILE:SDL3_ttf::SDL3_ttf>
    )
endif()

add_custom_command(TARGET Real-Time-Subtitler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${RTS_COPY_LIBS}
    $<TARGET_FILE_DIR:Real-Time-Subtitler>
)

# copy icon asset next to the exe
add_custom_command(TARGET Real-Time-Subtitler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/placeholder_rts_icon.png"
    $<TARGET_FILE_DIR:Real-Time-Subtitler>
)


# copy fonts/ directory next to the exe
add_custom_command(TARGET Real-Time-Subtitler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/fonts"
    $<TARGET_FILE_DIR:Real-Time-Subtitler>/fonts
)
# copy models/ only if it hasn't already been copied and filter out download-model scripts
if(NOT EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models")
    file(COPY "${CMAKE_SOURCE_DIR}/models/"
         DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models"
         FILES_MATCHING PATTERN "*.bin"
    )
endif()