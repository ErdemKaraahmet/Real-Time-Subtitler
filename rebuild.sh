#!/bin/bash
set -e  # Exit on any error
set -x  # Print each command before running it

echo "hi"
if [ -d "build" ]; then #removing prev build
    rm -rf build #relative path 
    echo "prev build removed"
fi 
#closes if statement

cmake -B build
echo "configured cmake"

cmake --build build
echo "build succesfull"
echo "run with ./build/auto-subtitler.exe"