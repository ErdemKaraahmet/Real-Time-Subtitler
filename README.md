# Real-Time-Subtitler

A lightweight, cross-platform real-time subtitle clickthrough overlay that captures system audio and converts it to live subtitles. Which hopefully will be a useful alternative to native Windows and Chrome live captions, as well as existing Linux and macOS applications, for individuals with hearing impairment.

## Prerequisites

### On Windows

- A C/C++ compiler with MinGW (e.g., [MSYS2](https://www.msys2.org/) UCRT64 toolchain), MSVC not supported yet
- CMake 3.24+
- Git

For GPU Acceleration the build automatically detects the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). If found, whisper.cpp will be compiled with Vulkan GPU acceleration for faster inference. If not found, it falls back to CPU only.

### On Linux (Debian / Ubuntu)

Install the compiler toolchain and SDL3 dependencies:
```bash
sudo apt install build-essential git make pkg-config cmake ninja-build \
    libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev \
    libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libusb-1.0-0-dev
```

For GPU Acceleration build automatically detects the Vulkan development headers, shader compiler, and spec headers. If not found, it falls back to CPU only. Install with:
```bash
sudo apt install libvulkan-dev glslc spirv-headers
```

### On macOS

Install CMake and Git:
```bash
brew install cmake git
```
*(Metal GPU acceleration is automatically enabled on Apple Silicon)*

## Build
```bash
git clone --recurse-submodules https://github.com/ErdemKaraahmet/Real-Time-Subtitler.git
cd Real-Time-Subtitler
cmake -B build
cmake --build build
```

## Models

To download Whisper models, use the helper script provided by `whisper.cpp`.

### On Windows (PowerShell/CMD)
Use the native `.cmd` script:
```powershell
# List available models
.\deps\whisper.cpp\models\download-ggml-model.cmd

# Fastest but lower accuracy
.\deps\whisper.cpp\models\download-ggml-model.cmd tiny.en-q5_1 models

# Best balance
.\deps\whisper.cpp\models\download-ggml-model.cmd base.en-q5_1 models

# Highest accuracy but slowest
.\deps\whisper.cpp\models\download-ggml-model.cmd large-v3 models
```

### On Linux, macOS, or Git Bash
Use the `.sh` script:
```bash
# List available models
./deps/whisper.cpp/models/download-ggml-model.sh

# Fastest but lower accuracy
./deps/whisper.cpp/models/download-ggml-model.sh tiny-q8_0 models/

# Best Balance
./deps/whisper.cpp/models/download-ggml-model.sh base.en models/

# Highest accuracy but slowest
./deps/whisper.cpp/models/download-ggml-model.sh large-v3 models/
```



## Run
```bash
./bin/Real-Time-Subtitler
```

## Testing

For convenience during development, helper scripts are provided that build the project, attempt to play the first MP3 file found in the `bin/` directory as a test audio source, and launch the application:

- **Windows (PowerShell)**: `./build_and_test.ps1`
- **Linux / MSYS2 (Bash)**: `./build_and_test.sh` 
*both (Requires `ffplay` from FFmpeg)*

## Benchmark Logging

Build with `-DRTS_BENCH=ON` to log per-inference latency and model confidence to `bench/rts_bench.csv`:

```bash
cmake -B build -S . -DRTS_BENCH=ON
cmake --build build --config Release
```

## Configuration

Real-Time-Subtitler includes a **graphical Control Panel** that allows you to adjust settings on the fly. 
To access it, right-click the RTS icon in your system tray and select **Control Panel**. 

From the Control Panel you can:
- Change the subtitle **Font** (loads fonts from the `fonts/` directory)
- Adjust **Font Size** and **Outline Thickness**
- Pick custom **Text** and **Outline Colors** with a live preview
- Hot-swap the active **Whisper Model** (loads models from the `models/` directory)
- **Pause/Resume** transcription or **Move Window** to drag the subtitle overlay around your screen.

*Settings are automatically saved to `bin/config.ini` when you hit Save.*

## Dependencies

> All dependencies listed are bundled as submodules — no separate installation required.

- SDL3
- SDL3_ttf
- whisper.cpp
- cimgui (Dear ImGui)

> A patch to SDL3 to enable mouse passthrough functionality will be automatically applied ([SDL PR #14561](https://github.com/libsdl-org/SDL/pull/14561) by [AQtun81](https://github.com/AQtun81)). This patch will be removed from the build process once it is merged into the official SDL3 release.


## To-Do

- [x] Clickthrough transparent text overlay (might not work properly for macOS)
- [x] Graphical Control Panel for UI customization
- [x] Configurable font, color, and outline
- [x] System audio capture (Windows)
- [x] System audio capture (Linux)
- [x] System audio capture (macOS, only captures microphone for now)
- [x] Live Whisper transcription
- [ ] Translation
- [ ] Language selection
- [ ] Own model download script
- [ ] Model download from control panel
- [ ] Confidence-based text coloring
- [ ] Speaker diarization
- [x] Multiple model support
