@echo off
echo Generating a Visual Studio 2026 x64 solution. Requires CMake and Visual Studio 2026 C++ tools.
cmake -S "%~dp0." -B "%~dp0build" -G "Visual Studio 18 2026" -A x64
exit /b %errorlevel%
