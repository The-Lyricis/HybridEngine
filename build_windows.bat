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
echo !C_WHITE!  H   H  Y   Y  BBBB   RRRR   IIIII  DDDD        EEEEE  N   N  GGGG   IIIII  N   N  EEEEE !C_RESET!
echo !C_WHITE!  H   H   Y Y   B   B  R   R    I    D   D       E      NN  N  G        I    NN  N  E     !C_RESET!
echo !C_WHITE!  HHHHH    Y    BBBB   RRRR     I    D   D       EEEE   N N N  G  GG    I    N N N  EEEE  !C_RESET!
echo !C_WHITE!  H   H    Y    B   B  R R      I    D   D       E      N  NN  G   G    I    N  NN  E     !C_RESET!
echo !C_WHITE!  H   H    Y    BBBB   R  RR  IIIII  DDDD        EEEEE  N   N  GGGG   IIIII  N   N  EEEEE !C_RESET!
echo !C_CYAN!===========================================================================================!C_RESET!
echo !C_GREEN!version 0.0.2!C_RESET!
echo !C_CYAN!===========================================================================================!C_RESET!
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
