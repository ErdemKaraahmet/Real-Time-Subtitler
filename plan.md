# Implement In-App Model Downloader

This plan outlines how we can add a feature to the Control Panel allowing users to download Whisper models directly from the UI, bypassing the need for terminal scripts.

## Dependency Strategy
To avoid bloating the application with networking dependencies like `libcurl` or dealing with complex Windows WinHTTP APIs, this plan proposes using Windows' built-in `curl.exe` (included natively in all Windows 10/11 installations). The app will spawn a background thread that executes the `curl` command. Let me know if you are comfortable with this approach.

## Open Questions

**Model Selection List**: The official bash script lists over 30 variations of models (different sizes, English-only vs multilingual, various quantization levels). 
For the UI, should we provide a massive dropdown with **all** 30+ options, or a **curated list** of the most practical ones (e.g., `tiny-q8_0`, `base.en-q8_0`, `small.en-q8_0`, `medium.en-q8_0`)?

## Proposed Changes

### UI & Threading Architecture (Control Panel)

#### [MODIFY] `include/controlPanel.h`
- Declare shared state variables for the download process (e.g., `extern bool isDownloading;`, `extern char downloadStatus[128];`).

#### [MODIFY] `src/controlPanel.c`
- **UI Additions**: Add a new sub-section under Model Selection called "Download Model". This will feature a dropdown to select the model to download, and a "Download" button.
- **Thread Spawning**: When "Download" is clicked, disable the button, set `isDownloading = true`, and spawn an `SDL_Thread` (e.g., `SDL_CreateThread(downloadWorker, ...)`).
- **Worker Logic**: 
  - The worker thread will execute `system("curl.exe -s -L --output models/ggml-<model>.bin <HuggingFace_URL>")`.
  - Upon completion, the thread will verify if the file exists and update the `downloadStatus` string.
  - The thread will trigger the existing `scanModelsDirectory()` function so the newly downloaded model instantly populates in the standard Model selection dropdown.
- **Progress Feedback**: Display the `downloadStatus` string in the UI so the user knows it is working in the background.

## Verification Plan

### Manual Verification
1. Open the Control Panel.
2. Select a model that isn't currently downloaded (e.g., `tiny.en-q8_0`) from the new downloader dropdown.
3. Click "Download" and ensure the UI does not freeze (thanks to the background thread).
4. Verify the model appears in the main "Model" dropdown automatically when finished.
5. Select the new model, click Save, and ensure speech-to-text works using the newly downloaded file.

## References (whisper.cpp Repository)

The download URLs and available model list for this plan were derived directly from the official `whisper.cpp` bash script located in our repository at:
**[deps/whisper.cpp/models/download-ggml-model.sh](file:///D:/CodeProjects/RTS/deps/whisper.cpp/models/download-ggml-model.sh)**

Specifically, this script defines:
- **Base URL**: `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml`
- **File Format**: `ggml-<model_name>.bin` (e.g. `ggml-tiny-q8_0.bin`)
- **Tiny Diarize Variation**: If the model has `tdrz`, it switches to `https://huggingface.co/akashmjn/tinydiarize-whisper.cpp/resolve/main/ggml`
