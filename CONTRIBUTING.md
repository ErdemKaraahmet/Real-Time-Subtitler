# Contributing to Real-Time Subtitler (RTS)

Thank you for your interest in contributing to **Real-Time Subtitler (RTS)**! This guide outlines how to set up your environment, build and test locally, run dynamic analysis, and submit clean pull requests. Read [README.md](README.md) before reading here.

---

## 1. Codebase Structure & Standards

### Technical Requirements
- **C Standard:** C11
- **C++ Standard:** C++17
- **Minimum CMake Version:** 3.24

### Core Components (`src/` and `include/`)
- **`src/main.c`**: Entry point, main thread loop, event handling, and worker thread orchestration.
- **`src/audioCapture.c`**: Captures system loopback audio via miniAudio into a thread-safe ring buffer.
- **`src/whisperEngine.c`**: Manages whisper.cpp context, model loading, and Vulkan GPU-accelerated inference runs.
- **`src/windowManager.c`**: Handles transparent borderless window creation, positioning, and mouse passthrough.
- **`src/textTexture.c`**: Generates SDL text textures with custom fonts, colors, and outline styles using SDL3_ttf.
- **`src/controlPanel.c`**: Graphical settings overlay built with Dear ImGui (`cimgui`).
- **`src/modelManager.c`**: Handles model catalog fetching, multi-threaded downloading, ETA calculation, and SHA-256 verification.
- **`src/configManager.c`**: Handles `config.json` parsing and persistence via cJSON, with automatic fallback defaults and corrupted-file backup (`.bak`).
- **`src/trayManager.c`**: Manages the cross-platform system tray icon and context menus.

---

## 2. CMake Options Reference

| Option | Default | Description |
|:---|:---|:---|
| **`RTS_VERSION`** | `"0.0.0"` | Sets the application version string (e.g. `-DRTS_VERSION="1.2.3"`), used to dynamically set version on release |
| **`RTS_BENCH`** | `OFF` | Enables per-inference latency, model confidence and token count logging to `bench/rts_bench.csv` |
| **`RTS_ENABLE_SANITIZERS`** | `OFF` | Enables AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) |
| **`RTS_ENABLE_TSAN`** | `OFF` | Enables ThreadSanitizer (TSan) for data race detection |
| **`RTS_CLANG_TIDY`** | `OFF` | Runs `clang-tidy` on project files during compilation |

---

## 3. Local Testing & Dynamic Analysis

Before submitting a Pull Request, you must test your changes locally. You can use the helper scripts `build_and_run.sh` or `build_and_run.ps1` to aid with testing.

These scripts:
- Format your changes using `clang-format`.
- Build with specified options and launches Real-Time Subtitler.
- Play the first MP3 in the `bin/` folder with `ffplay` (copied from `deps/whisper.cpp/samples/jfk.mp3` if no MP3 exists in `bin/`).

```bash
# Standard build & run with sample audio:
./build_and_run.sh

# Run with AddressSanitizer + UndefinedBehaviorSanitizer (ASan + UBSan):
./build_and_run.sh -s

# Run with ThreadSanitizer (TSan):
./build_and_run.sh -t

# Run with local Cppcheck static analysis:
./build_and_run.sh -c

# Run with Clang-Tidy static analysis during build:
./build_and_run.sh -l

# Combine multiple developer flags:
./build_and_run.sh -s -c -l
```

On Windows (PowerShell):
```powershell
./build_and_run.ps1 -s -c -l
```

### Direct Manual Tool Execution

```bash
# Run Clang-Tidy on local git diff only:
git diff -U0 -- 'src/*.[ch]' 'include/*.[ch]' | clang-tidy-diff -p1 -path build

# Run Cppcheck static analysis manually:
cppcheck --enable=warning,style,performance,portability --check-level=exhaustive --inline-suppr --error-exitcode=1 -I include/ -I src/ src/ include/
```

---

## 4. Code Formatting & Git Hooks

Code style is managed by **`.clang-format`**.

### Automatic Pre-Commit Formatting
The repository contains a Git pre-commit hook in `.githooks/pre-commit`. When configured by CMake, Git automatically runs `clang-format -i` on all modified `.c` and `.h` files prior to creating a commit.

To manually re-format all project source files in place:

```bash
find src/ include/ -type f \( -name "*.c" -o -name "*.h" \) ! -name "imgui_impl_sdlrenderer3_c.cpp" | xargs clang-format -i
```

---

## 5. CI/CD Workflows

### Lint Workflow
Every Push and Pull Request targeting `main` automatically triggers the **`Lint`** workflow ([`.github/workflows/lint.yml`](.github/workflows/lint.yml)) running three parallel jobs:

1. **`cppcheck`**: Verifies memory management and code safety.
2. **`clang-format`**: Checks code formatting (`clang-format --dry-run --Werror`).
3. **`clang-tidy-diff`**: Runs `clang-tidy` analysis exclusively on changed lines in the PR diff.

### Automated Release Workflow
Cross-platform release packages (Windows MSYS2 UCRT64, Linux Ubuntu, and macOS) are automatically compiled and published by [`.github/workflows/release.yaml`](.github/workflows/release.yaml) whenever a Git tag starting with `v` is pushed:

```bash
# Trigger a cross-platform release build
git tag v1.0.0
git push origin v1.0.0
```

---

## 6. Submitting a Pull Request

1. Ensure your code builds without compiler warnings or errors.
2. Test locally under ASan/UBSan (`./build_and_run.sh -s`).
3. Test locally under TSan (`./build_and_run.sh -t`).
4. Ensure Git submodules remain unchanged.
5. Write descriptive commit messages following conventional standards (e.g. `feat(audio): ...` or `fix(ui): ...`).
