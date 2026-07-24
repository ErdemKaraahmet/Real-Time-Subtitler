#!/bin/bash
# Builds and launches Real-Time Subtitler with sample audio (requires ffplay).
# Options: -s / --sanitizers (ASan+UBSan), -t / --tsan, -c / --cppcheck, -l / --tidy

USE_SANITIZERS=OFF
USE_TSAN=OFF
RUN_CPPCHECK=OFF
USE_TIDY=OFF

for arg in "$@"; do
    case $arg in
        --sanitizers|-s)
            USE_SANITIZERS=ON
            ;;
        --tsan|-t)
            USE_TSAN=ON
            ;;
        --cppcheck|-c)
            RUN_CPPCHECK=ON
            ;;
        --tidy|-l)
            USE_TIDY=ON
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
                 --inline-suppr \
                 --error-exitcode=1 \
                 -I include/ -I src/ \
                 src/ include/
    else
        echo "Warning: cppcheck is not installed."
    fi
fi

# Dynamically reconfigure CMake based on active combinations
if [ "$USE_SANITIZERS" = "ON" ] || [ "$USE_TSAN" = "ON" ] || [ "$USE_TIDY" = "ON" ]; then
    echo "Reconfiguring build options (Sanitizers: $USE_SANITIZERS, TSan: $USE_TSAN, Clang-Tidy: $USE_TIDY)..."
    cmake -B build -S . \
        -DRTS_ENABLE_SANITIZERS=$USE_SANITIZERS \
        -DRTS_ENABLE_TSAN=$USE_TSAN \
        -DRTS_CLANG_TIDY=$USE_TIDY
fi

cmake --build build -j $(nproc)

# copy default sample if no test MP3 exists
ls bin/*.mp3 &>/dev/null || cp deps/whisper.cpp/samples/jfk.mp3 bin/jfk.mp3 2>/dev/null

# Find and play the first MP3 file found in the bin directory
mp3_files=(bin/*.mp3)
if [ -f "${mp3_files[0]}" ]; then
    nohup ffplay -v 0 -nodisp -autoexit "${mp3_files[0]}" > /dev/null 2>&1 &
else
    echo "No MP3 files found in bin/ to play."
fi

./bin/Real-Time-Subtitler