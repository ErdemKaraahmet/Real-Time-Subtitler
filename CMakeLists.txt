cmake_minimum_required(VERSION 3.20)
project(Real-Time-Subtitler C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Build SDL3 from source
add_subdirectory(deps/SDL)

add_executable(Real-Time-Subtitler
    src/transparentWindow.c
)

target_link_libraries(Real-Time-Subtitler SDL3::SDL3)

# Copy SDL3.dll next to the exe automatically
add_custom_command(TARGET Real-Time-Subtitler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3>
    $<TARGET_FILE_DIR:Real-Time-Subtitler>
)