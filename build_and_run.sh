#!/bin/bash

# Formats the current changes using clang-format.
# Builds with specified options and launches Real-Time Subtitler.
# Plays the first mp3 in the bin/ folder with ffplay, sample copied from deps/whisper.cpp/samples/jfk.mp3 if no mp3 exists in bin/

# Options: -s / --sanitizers (ASan+UBSan)
#          -t / --tsan (TSan)
#          -c / --cppcheck
#          -l / --tidy (clang-tidy)
#          -m / --monkey <seed> (Event monkey testing harness)

USE_SANITIZERS=OFF
USE_TSAN=OFF
RUN_CPPCHECK=OFF
USE_TIDY=OFF
USE_MONKEY=OFF
MONKEY_SEED=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --sanitizers|-s)
            USE_SANITIZERS=ON
            shift
            ;;
        --tsan|-t)
            USE_TSAN=ON
            shift
            ;;
        --cppcheck|-c)
            RUN_CPPCHECK=ON
            shift
            ;;
        --tidy|-l)
            USE_TIDY=ON
            shift
            ;;
        --monkey|-m)
            USE_MONKEY=ON
            if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
                MONKEY_SEED="$2"
                shift
            fi
            shift
            ;;
        *)
            shift
            ;;
    esac
done

if [ "$USE_SANITIZERS" = "ON" ] && [ "$USE_TSAN" = "ON" ]; then
    echo "Error: Cannot combine -s (Sanitizers) and -t (TSan). Choose one."
    exit 1
fi

# Auto-format modified, staged, and untracked C/header files in src/ and include/
if command -v clang-format >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    CHANGED_FILES=$(git status --porcelain | awk '{print $2}' | grep -E '^(src/|include/).*\.(c|h)$')
    if [ -n "$CHANGED_FILES" ]; then
        echo "Auto-formatting modified project files with clang-format..."
        echo "$CHANGED_FILES" | xargs clang-format -i
    fi
fi

if [ "$RUN_CPPCHECK" = "ON" ]; then
    if command -v cppcheck >/dev/null 2>&1; then
        echo "Running Cppcheck analysis..."
        cppcheck --enable=warning,style,performance,portability \
                 --check-level=exhaustive \
                 --inline-suppr \
                 --error-exitcode=1 \
                 -i deps/ \
                 --suppress=*:deps/* \
                 -I include/ -I src/ \
                 src/ include/
    else
        echo "Warning: cppcheck is not installed."
    fi
fi

# Configure runtime sanitizer suppressions for third-party dependencies
export TSAN_OPTIONS="suppressions=$(pwd)/sanitizers/tsan_suppressions.txt:second_deadlock_stack=1"
export UBSAN_OPTIONS="suppressions=$(pwd)/sanitizers/ubsan_suppressions.txt:print_stacktrace=1"
export LSAN_OPTIONS="suppressions=$(pwd)/sanitizers/lsan_suppressions.txt"
export ASAN_OPTIONS="detect_leaks=1:symbolize=1"

CURRENT_MONKEY=""
if [ -f "build/CMakeCache.txt" ]; then
    CURRENT_MONKEY=$(grep "RTS_MONKEY_TEST:BOOL=" build/CMakeCache.txt 2>/dev/null | cut -d= -f2)
fi

# Dynamically reconfigure CMake based on active combinations
if [ "$USE_SANITIZERS" = "ON" ] || [ "$USE_TSAN" = "ON" ] || [ "$USE_TIDY" = "ON" ] || [ "$USE_MONKEY" = "ON" ] || [ "$CURRENT_MONKEY" = "ON" ]; then
    echo "Reconfiguring build options (Sanitizers: $USE_SANITIZERS, TSan: $USE_TSAN, Clang-Tidy: $USE_TIDY, Monkey: $USE_MONKEY)..."
    cmake -B build -S . \
        -DRTS_ENABLE_SANITIZERS=$USE_SANITIZERS \
        -DRTS_ENABLE_TSAN=$USE_TSAN \
        -DRTS_CLANG_TIDY=$USE_TIDY \
        -DRTS_MONKEY_TEST=$USE_MONKEY
fi

cmake --build build -j $( (command -v nproc >/dev/null 2>&1 && nproc) || (command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu) || echo ${NUMBER_OF_PROCESSORS:-1} )

# copy default sample if no test MP3 exists
ls bin/*.mp3 &>/dev/null || cp deps/whisper.cpp/samples/jfk.mp3 bin/jfk.mp3 2>/dev/null

# Find and play the first MP3 file found in the bin directory
mp3_files=(bin/*.mp3)
if [ -f "${mp3_files[0]}" ]; then
    nohup ffplay -v 0 -nodisp -autoexit "${mp3_files[0]}" > /dev/null 2>&1 &
else
    echo "No MP3 files found in bin/ to play."
fi

if [ "$USE_MONKEY" = "ON" ]; then
    if [ -n "$MONKEY_SEED" ]; then
        ./bin/Real-Time-Subtitler --monkey "$MONKEY_SEED"
    else
        ./bin/Real-Time-Subtitler --monkey
    fi
else
    ./bin/Real-Time-Subtitler
fi