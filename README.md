# Auto-Subtitler

A lightweight, cross-platform real-time subtitle overlay that captures system audio and converts it to live subtitles.

## Quick Start

### Clone the Repository
```bash
git clone https://github.com/user/auto-subtitler.git
cd auto-subtitler
```

### Install Dependencies & Build

#### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake build-essential libsdl2-dev libsdl2-ttf-dev
cmake -B build && cmake --build build
./build/auto-subtitler
```

Or use the automated script:
```bash
chmod +x setup-and-run.sh
./setup-and-run.sh
```

#### macOS
```bash
brew install cmake sdl2 sdl2_ttf
cmake -B build && cmake --build build
./build/auto-subtitler
```

#### Windows (PowerShell / Git Bash)

**Option 1: Using MSYS2 / MinGW**
```powershell
# Install dependencies via MSYS2 pacman (if using MSYS2):
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-gcc

# Build
cmake -B build -G "MinGW Makefiles"
cmake --build build
.\build\auto-subtitler.exe
```

**Option 2: Using Chocolatey**
```powershell
choco install cmake mingw
# Download SDL2 dev libraries manually or use vcpkg (see advancing setup)
cmake -B build -G "MinGW Makefiles"
cmake --build build
.\build\auto-subtitler.exe
```

---

## Development Workflow

### Edit → Build → Run

After making changes to source code:
```bash
# Rebuild (only recompiles changed files)
cmake --build build

# Run
./build/auto-subtitler
```

### Clean Build
```bash
rm -rf build
cmake -B build
cmake --build build
```

---

## Project Structure

```
auto-subtitler/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── src/
│   └── main.c              # Entry point
└── build/                  # Build output (generated)
```

---

## Features (Planned)

- Real-time subtitle overlay
- Cross-platform (Windows, macOS, Linux)
- Low memory footprint
- Open-source

---

## License

MIT License

---

## Support

For issues and questions, open a GitHub issue or discussion.
