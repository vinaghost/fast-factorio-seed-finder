@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR%"

if "%~1"=="" (
    set "CONFIG=Release"
) else (
    set "CONFIG=%~1"
)

if "%~2"=="" (
    set "BUILD_DIR=%SCRIPT_DIR%build"
) else (
    set "BUILD_DIR=%~2"
)

if "%~3"=="" (
    set "GENERATOR=Ninja"
) else (
    set "GENERATOR=%~3"
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    echo Install CMake 3.30 or newer and try again.
    exit /b 1
)

echo Configuring project...
cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "%GENERATOR%"
if errorlevel 1 goto :fail

echo Building project...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" -j
if errorlevel 1 goto :fail

echo Build succeeded.
exit /b 0

:fail
echo Build failed.
exit /b 1