# Real-Time-Subtitler

A lightweight, cross-platform real-time subtitle clickthrough overlay that captures system audio and converts it to live subtitles.

Which hopefully will be a usefull alternative to native windows and chrome live captions for inviduals with hearing impairment.


## Build
```bash
git clone --recurse-submodules https://github.com/ErdemKaraahmet/Real-Time-Subtitler.git
cmake -B build
cmake --build build
```

> A patch to SDL3 to enable mouse passthrough functionality will be automatically applied ([SDL PR #14561](https://github.com/libsdl-org/SDL/pull/14561) by [AQtun81](https://github.com/AQtun81)). This patch will be removed from the build process once it is merged into the official SDL3 release.

## Dependencies

- SDL3

## Features (Planned)

- Real-time subtitle clickthrough overlay
- Cross-platform (Windows, macOS, Linux)
- Low memory footprint

