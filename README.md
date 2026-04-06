# Real-Time-Subtitler

A lightweight, cross-platform real-time subtitle clickthrough overlay that captures system audio and converts it to live subtitles.

Which hopefully will be a useful alternative to native Windows and Chrome live captions for individuals with hearing impairment.

## Prerequisites

- [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
- CMake 3.20+
- Git

## Build
```bash
git clone --recurse-submodules https://github.com/ErdemKaraahmet/Real-Time-Subtitler.git
cd Real-Time-Subtitler
cmake -B build
cmake --build build
```

## Models

List available Whisper models:
```bash
bash deps/whisper.cpp/models/download-ggml-model.sh
```

Download a Whisper model into the `models/` folder:
```bash
# Recommended: fast with good accuracy
bash deps/whisper.cpp/models/download-ggml-model.sh tiny.en-q8_0 models/

# Higher accuracy, slower
bash deps/whisper.cpp/models/download-ggml-model.sh base.en models/
```

## Run
```bash
./bin/Real-Time-Subtitler
```

## Configuration

Edit `bin/config.ini` to customize the overlay:

| Key | Default | Description |
|-----|---------|-------------|
| `font_size` | `36` | Font size in points |
| `outline_thickness` | `4` | Text outline thickness in pixels |
| `text_color` | `255,255,255` | Subtitle text color (R,G,B) |
| `text_outline_color` | `0,0,0` | Outline color (R,G,B) |
| `modelPath` | `models/ggml-base.en.bin` | Path to Whisper model |

## Dependencies

>All dependencies listed are bundled as submodules — no separate installation required.

- SDL3
- SDL3_ttf
- whisper.cpp

> A patch to SDL3 to enable mouse passthrough functionality will be automatically applied ([SDL PR #14561](https://github.com/libsdl-org/SDL/pull/14561) by [AQtun81](https://github.com/AQtun81)). This patch will be removed from the build process once it is merged into the official SDL3 release.


## To-Do

- [x] Clickthrough transparent text overlay
- [x] Configurable font, color, and outline
- [x] System audio capture (Windows)
- [ ] System audio capture (Linux)
- [ ] System audio capture (macOS)
- [ ] Live Whisper transcription (working, latency improvements needed)
- [ ] Confidence-based text coloring
- [ ] Speaker diarization
- [X] Multiple model support