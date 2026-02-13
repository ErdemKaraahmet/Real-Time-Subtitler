cmake_minimum_required(VERSION 3.15)
project(auto-subtitler C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(auto-subtitler src/main.c)

# Project-local include and lib folders
set(PROJ_ROOT "${CMAKE_SOURCE_DIR}")
set(EXTRA_INCLUDE "${PROJ_ROOT}/include")   # path that contains SDL3/ or SDL.h
set(EXTRA_LIB    "${PROJ_ROOT}/lib")       # path that contains libSDL3.a

target_include_directories(auto-subtitler PRIVATE ${EXTRA_INCLUDE})

# Prefer importing the static library as an imported target
add_library(SDL3_static STATIC IMPORTED)
set_target_properties(SDL3_static PROPERTIES
  IMPORTED_LOCATION "${EXTRA_LIB}/libSDL3.a"      # adjust filename if different
  INTERFACE_INCLUDE_DIRECTORIES "${EXTRA_INCLUDE}"
)

# Link the imported SDL3 static library and common Win32 system libs
target_link_libraries(auto-subtitler PRIVATE SDL3_static opengl32 gdi32 user32 dwmapi)


