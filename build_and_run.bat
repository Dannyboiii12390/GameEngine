@echo off
setlocal

REM Update "GameEngine.sln" to your actual solution file name.
set SLN_FILE="GameEngine.sln"

echo Locating MSBuild...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: vswhere.exe not found! Is Visual Studio installed?
    pause
    exit /b 1
)

REM Find the latest MSBuild available
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set MSBUILD_PATH="%%i"
)

if not defined MSBUILD_PATH (
    echo Error: MSBuild not found!
    pause
    exit /b 1
)

echo Compiling the project using %MSBUILD_PATH%...
%MSBUILD_PATH% %SLN_FILE% /p:Configuration=Release /p:Platform=x64 /t:Build

if %ERRORLEVEL% equ 0 (
    echo Compilation successful. Running Engine...
    
    REM Ensure the working directory is correct so relative shader paths work
    REM Try pointing it to the Engine directory where SHADERS resides
    cd Engine
    start ..\x64\Release\Engine.exe
) else (
    echo Compilation failed!
    pause
)