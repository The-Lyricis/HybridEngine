@echo off
setlocal EnableExtensions EnableDelayedExpansion

chcp 65001 >nul

for /f %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"

set "C_RESET=!ESC![0m"
set "C_CYAN=!ESC![96m"
set "C_WHITE=!ESC![97m"
set "C_DIM=!ESC![90m"
set "C_YELLOW=!ESC![93m"
set "C_GREEN=!ESC![92m"
set "C_RED=!ESC![91m"

cls
echo !C_CYAN!===========================================================================================!C_RESET!
echo !C_WHITE!  H   H  Y   Y  BBBB   RRRR   IIIII  DDDD        EEEEE  N   N   GGG   IIIII  N   N  EEEEE !C_RESET!
echo !C_WHITE!  H   H   Y Y   B   B  R   R    I    D   D       E      NN  N  G        I    NN  N  E     !C_RESET!
echo !C_WHITE!  HHHHH    Y    BBBB   RRRR     I    D   D       EEEE   N N N  G  GG    I    N N N  EEEE  !C_RESET!
echo !C_WHITE!  H   H    Y    B   B  R R      I    D   D       E      N  NN  G   G    I    N  NN  E     !C_RESET!
echo !C_WHITE!  H   H    Y    BBBB   R  RR  IIIII  DDDD        EEEEE  N   N   GGG   IIIII  N   N  EEEEE !C_RESET!
echo !C_CYAN!===========================================================================================!C_RESET!
echo !C_GREEN!version 0.1.0!C_RESET!
echo !C_CYAN!===========================================================================================!C_RESET!
echo.

set "CMAKE_EXE="
set "VS_INSTALL_PATH="
set "BUILD_DIR=build\ninja-msvc"
set "BUILD_TYPE=Release"

call :find_visual_studio
call :find_cmake

if not defined CMAKE_EXE (
    echo !C_RED![ERROR] CMake was not found.!C_RESET!
    call :print_install_guide
    goto :end
)

echo !C_DIM!Using CMake: !CMAKE_EXE!!C_RESET!
echo.

call :setup_msvc_env
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] Visual Studio C++ environment could not be initialized.!C_RESET!
    echo Missing Visual Studio 2022 or C++ build tools.
    call :print_install_guide
    goto :end
)

echo !C_DIM!Using compiler:!C_RESET!
where cl
echo.

call :ensure_ninja
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] Ninja was not found.!C_RESET!
    call :print_install_guide
    goto :end
)

echo !C_DIM!Using Ninja:!C_RESET!
where ninja
echo.

call :check_windows_sdk_tools
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] Windows SDK tools were not found.!C_RESET!
    echo Missing rc.exe or mt.exe.
    call :print_install_guide
    goto :end
)

echo !C_DIM!CMake version:!C_RESET!
"!CMAKE_EXE!" --version
echo.

echo !C_YELLOW![1/2] Configure...!C_RESET!

rem Clear CMake-related environment variables to avoid generator pollution.
set CMAKE_GENERATOR=
set CMAKE_GENERATOR_PLATFORM=
set CMAKE_GENERATOR_TOOLSET=

if exist "!BUILD_DIR!" (
    echo !C_YELLOW!Cleaning build directory: !BUILD_DIR!!C_RESET!
    rmdir /s /q "!BUILD_DIR!"
)

echo !C_DIM!Configure command:!C_RESET!
echo "!CMAKE_EXE!" -S . -B "!BUILD_DIR!" -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=!BUILD_TYPE!
echo.

"!CMAKE_EXE!" -S . -B "!BUILD_DIR!" -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=!BUILD_TYPE!

if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] CMake configure failed.!C_RESET!
    echo.
    echo !C_YELLOW!Diagnostic info:!C_RESET!
    echo   BUILD_DIR = !BUILD_DIR!
    echo   Generator = Ninja
    echo   BuildType = !BUILD_TYPE!
    echo   CMake     = !CMAKE_EXE!
    echo.
    echo Please check:
    echo   - cl.exe is available after VsDevCmd
    echo   - ninja.exe is available
    echo   - rc.exe and mt.exe are available
    echo   - Windows SDK is installed
    echo   - CMakeLists.txt does not force another generator or compiler
    goto :end
)

echo !C_YELLOW![2/2] Build (!BUILD_TYPE!)...!C_RESET!

"!CMAKE_EXE!" --build "!BUILD_DIR!" --config !BUILD_TYPE!
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] Build failed.!C_RESET!
    goto :end
)

echo.
echo !C_GREEN![OK] Build finished successfully.!C_RESET!

:end
echo.
pause
endlocal
exit /b


:find_visual_studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        set "VS_INSTALL_PATH=%%I"
        exit /b 0
    )

    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -property installationPath 2^>nul`) do (
        set "VS_INSTALL_PATH=%%I"
        exit /b 0
    )
)

exit /b 0


:find_cmake
rem Prefer Visual Studio bundled CMake.
if defined VS_INSTALL_PATH (
    if exist "!VS_INSTALL_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=!VS_INSTALL_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        exit /b 0
    )
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -property installationPath 2^>nul`) do (
        if exist "%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            exit /b 0
        )
    )
)

rem Fallback to PATH CMake.
for /f "delims=" %%A in ('where cmake 2^>nul') do (
    set "CMAKE_EXE=%%A"
    exit /b 0
)

exit /b 0


:setup_msvc_env
where cl >nul 2>nul
if not errorlevel 1 (
    echo !C_DIM!MSVC environment already initialized.!C_RESET!
    exit /b 0
)

if not defined VS_INSTALL_PATH (
    call :find_visual_studio
)

if defined VS_INSTALL_PATH (
    if exist "!VS_INSTALL_PATH!\Common7\Tools\VsDevCmd.bat" (
        echo !C_DIM!Initializing MSVC environment with VsDevCmd...!C_RESET!
        call "!VS_INSTALL_PATH!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
        where cl >nul 2>nul
        if not errorlevel 1 exit /b 0
    )

    if exist "!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
        echo !C_DIM!Initializing MSVC environment with vcvars64...!C_RESET!
        call "!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvars64.bat"
        where cl >nul 2>nul
        if not errorlevel 1 exit /b 0
    )
)

exit /b 1


:ensure_ninja
where ninja >nul 2>nul
if not errorlevel 1 exit /b 0

if defined VS_INSTALL_PATH (
    if exist "!VS_INSTALL_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
        set "PATH=!VS_INSTALL_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"
        where ninja >nul 2>nul
        if not errorlevel 1 exit /b 0
    )
)

exit /b 1


:check_windows_sdk_tools
where rc >nul 2>nul
if errorlevel 1 (
    echo !C_RED![ERROR] rc.exe was not found.!C_RESET!
    exit /b 1
)

where mt >nul 2>nul
if errorlevel 1 (
    echo !C_RED![ERROR] mt.exe was not found.!C_RESET!
    exit /b 1
)

echo !C_DIM!Using Windows SDK tools:!C_RESET!
where rc
where mt
echo.

exit /b 0


:print_install_guide
echo.
echo !C_YELLOW!Required Windows build environment:!C_RESET!
echo.
echo   1. Visual Studio 2022 Community or Visual Studio Build Tools 2022
echo   2. Workload: Desktop development with C++
echo   3. Components:
echo      - MSVC v143 C++ x64/x86 build tools
echo      - Windows 10 SDK or Windows 11 SDK
echo      - C++ CMake tools for Windows
echo   4. Ninja
echo.
echo !C_WHITE!Recommended installation steps:!C_RESET!
echo.
echo   Open Visual Studio Installer:
echo      Modify Visual Studio 2022
echo      Select "Desktop development with C++"
echo      Make sure Windows SDK is checked
echo.
echo   Optional command line tools:
echo      winget install --id Kitware.CMake -e
echo      winget install --id Ninja-build.Ninja -e
echo.
echo !C_WHITE!After installation:!C_RESET!
echo   Restart the terminal, then run build_windows.bat again.
echo.
exit /b 0