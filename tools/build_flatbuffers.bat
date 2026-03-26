@echo off
setlocal

:: Determine the path to flatc.exe (assuming it is in the same directory as this script)
set FLATC_DIR=%~dp0
set FLATC_EXE="%FLATC_DIR%flatc.exe"

:: Check if a file path was provided
if "%~1"=="" (
    echo Usage: build_flatbuffers.bat [path_to_fbs_file]
    exit /b 1
)

set FBS_FILE=%~1

:: Check if flatc.exe exists
if not exist %FLATC_EXE% (
    echo Error: flatc.exe not found in %FLATC_DIR%
    exit /b 1
)

echo Compiling %FBS_FILE%...
:: Compile the .fbs file to C++ (--cpp) and output it in the same directory as the source file
%FLATC_EXE% --cpp -o "%~dp1." "%FBS_FILE%"

if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)

echo Compilation successful.
exit /b 0