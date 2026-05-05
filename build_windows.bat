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
echo !C_GREEN!version 0.0.2!C_RESET!
echo !C_CYAN!===========================================================================================!C_RESET!
echo.

set "CMAKE_EXE="
set "VS_INSTALL_PATH="
set "NEED_CMAKE=0"
set "NEED_MSVC=0"

call :find_cmake
if not defined CMAKE_EXE (
    echo !C_RED![ERROR] CMake was not found.!C_RESET!
    set "NEED_CMAKE=1"
)

call :check_msvc
if errorlevel 1 (
    echo !C_RED![ERROR] Visual Studio C++ toolchain was not found.!C_RESET!
    set "NEED_MSVC=1"
)

if "!NEED_CMAKE!!NEED_MSVC!" NEQ "00" (
    call :print_prerequisites
    call :offer_install
    goto :end
)

echo !C_DIM!Using CMake: !CMAKE_EXE!!C_RESET!
echo.

echo !C_YELLOW![1/2] Configure...!C_RESET!
"!CMAKE_EXE!" -S . -B build
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] CMake configure failed.!C_RESET!
    goto :end
)

echo !C_YELLOW![2/2] Build (Release)...!C_RESET!
"!CMAKE_EXE!" --build build --config Release
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

:find_cmake
for /f "delims=" %%A in ('where cmake 2^>nul') do (
    set "CMAKE_EXE=%%A"
    exit /b 0
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath 2^>nul`) do (
        if exist "%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            exit /b 0
        )
    )
)
exit /b 0

:check_msvc
where cl >nul 2>nul
if not errorlevel 1 exit /b 0

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        set "VS_INSTALL_PATH=%%I"
        for /d %%T in ("%%I\VC\Tools\MSVC\*") do (
            if exist "%%T\bin\Hostx64\x64\cl.exe" exit /b 0
            if exist "%%T\bin\Hostx86\x86\cl.exe" exit /b 0
        )
    )

    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -property installationPath 2^>nul`) do (
        set "VS_INSTALL_PATH=%%I"
    )
)
exit /b 1

:print_prerequisites
echo.
echo !C_YELLOW!Hybrid Engine requires these Windows build tools:!C_RESET!
echo   - CMake 3.20 or newer
echo   - Visual Studio 2022 or Build Tools 2022 with "Desktop development with C++"
echo.
echo !C_WHITE!Recommended install options:!C_RESET!
echo   1. Visual Studio Installer:
echo      Install "Desktop development with C++" and include MSVC, Windows SDK, and CMake tools.
echo.
echo   2. Command line with winget:
echo      winget install --id Kitware.CMake -e
echo      winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
echo.
echo After installing, restart this terminal and run build_windows.bat again.
exit /b 0

:offer_install
where winget >nul 2>nul
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] winget was not found on this system.!C_RESET!
    echo Please install the tools manually with Visual Studio Installer or from the official installers.
    exit /b 1
)

echo.
choice /c YN /m "Install missing build tools with winget now"
if errorlevel 2 (
    echo.
    echo Installation skipped.
    exit /b 0
)

echo.
echo !C_YELLOW!Installing missing build tools. This may take a while and may ask for administrator permission.!C_RESET!

if "!NEED_CMAKE!"=="1" (
    echo.
    echo !C_YELLOW!Installing CMake...!C_RESET!
    winget install --id Kitware.CMake -e
    if errorlevel 1 (
        echo !C_RED![ERROR] CMake installation failed.!C_RESET!
        exit /b 1
    )
)

if "!NEED_MSVC!"=="1" (
    echo.
    echo !C_YELLOW!Installing Visual Studio Build Tools C++ workload...!C_RESET!
    call :install_msvc
    if errorlevel 1 exit /b 1
)

echo.
echo !C_GREEN![OK] Installation command finished.!C_RESET!
echo Restart this terminal, then run build_windows.bat again.
exit /b 0

:install_msvc
set "VS_SETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe"
if defined VS_INSTALL_PATH if exist "!VS_SETUP!" (
    echo Found Visual Studio installation:
    echo   !VS_INSTALL_PATH!
    echo Adding the C++ desktop workload to this installation...
    "!VS_SETUP!" modify --installPath "!VS_INSTALL_PATH!" --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive --norestart --wait
    if errorlevel 1 (
        echo !C_RED![ERROR] Visual Studio Installer failed to add the C++ workload.!C_RESET!
        exit /b 1
    )
    exit /b 0
)

winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
if errorlevel 1 (
    echo !C_RED![ERROR] Visual Studio Build Tools installation failed.!C_RESET!
    exit /b 1
)
exit /b 0
