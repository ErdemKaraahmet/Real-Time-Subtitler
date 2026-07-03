#!/bin/bash
# Builds the project, plays the first MP3 found in bin/ as a test audio source using ffplay, and launches the executable.
# Requires: ffplay (part of FFmpeg)
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

./bin/Real-Time-Subtitler.exe