# Builds the project, plays the first MP3 found in bin/ as a test audio source, and launches the executable.

# Build the project using CMake
cmake --build build -j $(nproc)

# copy default sample if no test MP3 exists
if (-not (Test-Path "bin/*.mp3")) {
    Copy-Item "deps/whisper.cpp/samples/jfk.mp3" "bin/jfk.mp3" -ErrorAction SilentlyContinue
}

# Find the first MP3 file in the bin directory
$mp3File = Get-ChildItem -Path "bin" -Filter "*.mp3" | Select-Object -First 1 -ExpandProperty FullName

if ($mp3File) {
    # Run ffplay asynchronously in the background
    Start-Process ffplay -ArgumentList "-v 0 -nodisp -autoexit `"$mp3File`"" -NoNewWindow
} else {
    Write-Host "No MP3 files found in bin/ to play."
}

# Run the executable
.\bin\Real-Time-Subtitler.exe
