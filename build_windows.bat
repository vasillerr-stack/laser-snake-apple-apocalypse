@echo off
chcp 65001 >nul
cd /d "%~dp0"

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto error

cmake --build build --config Release
if errorlevel 1 goto error

start "LASER SNAKE" "build\LASER_SNAKE.exe"
goto end

:error
echo.
echo Build failed. Check the CMake and compiler messages above.
pause

:end
