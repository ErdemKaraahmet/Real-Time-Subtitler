# Formats the current changes using clang-format.
# Builds with specified options and launches Real-Time Subtitler.
# Plays the first mp3 in the bin/ folder with ffplay, sample copied from deps/whisper.cpp/samples/jfk.mp3 if no mp3 exists in bin/

# Options: -s / -Sanitizers (ASan+UBSan)
#          -t / -TSan (TSan)
#          -c / -Cppcheck
#          -l / -Tidy (clang-tidy)

param(
    [Alias("s", "-sanitizers")]
    [switch]$Sanitizers,

    [Alias("t", "-tsan")]
    [switch]$TSan,

    [Alias("c", "-cppcheck")]
    [switch]$Cppcheck,

    [Alias("l", "-tidy")]
    [switch]$Tidy
)

if ($Sanitizers -and $TSan) {
    Write-Error "Cannot combine -Sanitizers (-s) and -TSan (-t). Choose one."
    exit 1
}

# Auto-format modified, staged, and untracked C/header files in src/ and include/
if (Get-Command clang-format -ErrorAction SilentlyContinue) {
    $changedFiles = git status --porcelain | ForEach-Object { $_.Substring(3) } | Where-Object { $_ -match '^(src/|include/).*\.(c|h)$' }
    if ($changedFiles) {
        Write-Host "Auto-formatting modified project files with clang-format..."
        foreach ($file in $changedFiles) {
            clang-format -i $file
        }
    }
}

if ($Cppcheck) {
    if (Get-Command cppcheck -ErrorAction SilentlyContinue) {
        Write-Host "Running Cppcheck analysis..."
        cppcheck --enable=warning,style,performance,portability --check-level=exhaustive --inline-suppr --error-exitcode=1 -i deps/ --suppress=*:deps/* -I include/ -I src/ src/ include/
    } else {
        Write-Host "Warning: cppcheck is not installed."
    }
}

# Configure runtime sanitizer suppressions for third-party dependencies
$env:TSAN_OPTIONS = "suppressions=$(Get-Location)/sanitizers/tsan_suppressions.txt:second_deadlock_stack=1"
$env:UBSAN_OPTIONS = "suppressions=$(Get-Location)/sanitizers/ubsan_suppressions.txt:print_stacktrace=1"
$env:LSAN_OPTIONS = "suppressions=$(Get-Location)/sanitizers/lsan_suppressions.txt"
$env:ASAN_OPTIONS = "detect_leaks=1:symbolize=1"

# Dynamically reconfigure CMake based on active combinations
if ($Sanitizers -or $TSan -or $Tidy) {
    $sanFlag = if ($Sanitizers) { "ON" } else { "OFF" }
    $tsanFlag = if ($TSan) { "ON" } else { "OFF" }
    $tidyFlag = if ($Tidy) { "ON" } else { "OFF" }

    Write-Host "Reconfiguring build options (Sanitizers: $sanFlag, TSan: $tsanFlag, Clang-Tidy: $tidyFlag)..."
    cmake -B build -S . -DRTS_ENABLE_SANITIZERS=$sanFlag -DRTS_ENABLE_TSAN=$tsanFlag -DRTS_CLANG_TIDY=$tidyFlag
}

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
