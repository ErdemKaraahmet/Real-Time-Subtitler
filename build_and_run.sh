#!/bin/bash
# Builds and launches Real-Time Subtitler with sample audio (requires ffplay).
# Options: -s / --sanitizers (ASan+UBSan), -t / --tsan

USE_SANITIZERS=OFF
USE_TSAN=OFF
for arg in "$@"; do
    case $arg in
        --sanitizers|-s)
            USE_SANITIZERS=ON
            ;;
        --tsan|-t)
            USE_TSAN=ON
            ;;
    esac
done

if [ "$USE_SANITIZERS" = "ON" ]; then
    echo "Reconfiguring build with AddressSanitizer & UndefinedBehaviorSanitizer enabled..."
    cmake -B build -S . -DRTS_ENABLE_SANITIZERS=ON -DRTS_ENABLE_TSAN=OFF
elif [ "$USE_TSAN" = "ON" ]; then
    echo "Reconfiguring build with ThreadSanitizer enabled..."
    cmake -B build -S . -DRTS_ENABLE_TSAN=ON -DRTS_ENABLE_SANITIZERS=OFF
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