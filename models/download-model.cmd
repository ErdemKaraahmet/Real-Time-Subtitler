rem Adapted from whisper.cpp
rem Source: https://github.com/ggerganov/whisper.cpp/blob/master/models/download-ggml-model.cmd

@echo off

rem Save the original working directory
set "orig_dir=%CD%"

rem Get the script directory
set "models_path=%~dp0"

rem Set the root path to be the parent directory of the script
for %%d in (%~dp0..) do set "root_path=%%~fd"

rem Count number of arguments passed to script
set argc=0
for %%x in (%*) do set /A argc+=1

set models=tiny tiny-q5_1 tiny-q8_0 ^
tiny.en tiny.en-q5_1 tiny.en-q8_0 ^
base base-q5_1 base-q8_0 ^
base.en base.en-q5_1 base.en-q8_0 ^
small small-q5_1 small-q8_0 ^
small.en small.en-q5_1 small.en-q8_0 small.en-tdrz ^
medium medium-q5_0 medium-q8_0 ^
medium.en medium.en-q5_0 medium.en-q8_0 ^
large-v1 ^
large-v2 large-v2-q5_0 large-v2-q8_0 ^
large-v3 large-v3-q5_0 ^
large-v3-turbo large-v3-turbo-q5_0 large-v3-turbo-q8_0

rem If argc is not equal to 1, print usage information and exit
if %argc% NEQ 1 (
  echo.
  echo Usage: download-ggml-model.cmd model
  CALL :list_models
  goto :eof
)

set model=%1

for %%b in (%models%) do (
  if "%%b"=="%model%" (
    CALL :download_model
    goto :eof
  )
)

echo Invalid model: %model%
CALL :list_models
goto :eof

:download_model
echo Downloading ggml model %model%...

if exist "%models_path%\\ggml-%model%.bin" (
  echo Model %model% already exists. Skipping download.
  goto :eof
)
echo %model% | findstr tdrz
if %ERRORLEVEL% neq 0 (
 PowerShell -NoProfile -ExecutionPolicy Bypass -Command "Start-BitsTransfer -Source https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-%model%.bin -Destination \"%models_path%\\ggml-%model%.bin\""
) else (
  PowerShell -NoProfile -ExecutionPolicy Bypass -Command "Start-BitsTransfer -Source https://huggingface.co/akashmjn/tinydiarize-whisper.cpp/resolve/main/ggml-%model%.bin -Destination \"%models_path%\\ggml-%model%.bin\""

)

if %ERRORLEVEL% neq 0 (
  echo Failed to download ggml model %model%
  echo Please try again later or download the original Whisper model files and convert them yourself.
  goto :eof
)

rem Check if 'whisper-cli' is available in the system PATH
where whisper-cli >nul 2>&1
if %ERRORLEVEL%==0 (
  rem If found, suggest 'whisper-cli' (relying on PATH resolution)
  set "whisper_cmd=whisper-cli"
) else (
  rem If not found, suggest the local build version
  set "whisper_cmd=%root_path%\build\bin\Release\whisper-cli.exe"
)

echo Done! Model %model% saved in %models_path%ggml-%model%.bin

goto :eof

:list_models
  echo.
  echo Available models:
  (for %%a in (%models%) do (
    echo %%a
  ))
  echo.
  exit /b
