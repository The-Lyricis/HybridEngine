@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM 可选：切到 UTF-8（如果你只输出英文，其实不需要）
chcp 65001 >nul

REM 取 ESC 字符，用于 ANSI 颜色序列
for /f %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"

REM 颜色定义
set "C_RESET=!ESC![0m"
set "C_CYAN=!ESC![96m"
set "C_WHITE=!ESC![97m"
set "C_DIM=!ESC![90m"
set "C_YELLOW=!ESC![93m"
set "C_GREEN=!ESC![92m"
set "C_RED=!ESC![91m"

cls
echo !C_CYAN!╔══════════════════════════════════════════════════════════════╗!C_RESET!
echo !C_CYAN!║!C_WHITE!  HYBRID ENGINE  -  BUILD SCRIPT                              !C_CYAN!║!C_RESET!
echo !C_CYAN!║!C_DIM!  CMake Configure + Build (Release)                           !C_CYAN!║!C_RESET!
echo !C_CYAN!╚══════════════════════════════════════════════════════════════╝!C_RESET!
echo.

echo !C_YELLOW![1/2] Configure...!C_RESET!
cmake -S . -B build
if errorlevel 1 (
    echo.
    echo !C_RED![ERROR] CMake configure failed.!C_RESET!
    goto :end
)

echo !C_YELLOW![2/2] Build (Release)...!C_RESET!
cmake --build build --config Release
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
