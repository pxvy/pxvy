@echo off
setlocal EnableExtensions

rem ===== user settings =====
set "MSYS2_ROOT=C:\msys64"
set "MSYS2_SHELL=%MSYS2_ROOT%\msys2_shell.cmd"
set "WORKDIR=%~dp0build_mediainfo_work"
set "INSTALL_DIR=%WORKDIR%\install"

rem ===== bash 에서 쓸 경로 형식으로 변환 =====
set "WORKDIR_SH=%WORKDIR:\=/%"
set "INSTALL_DIR_SH=%INSTALL_DIR:\=/%"

if not exist "%MSYS2_SHELL%" (
    echo [ERROR] not found: %MSYS2_SHELL%
    exit /b 1
)

if not exist "%WORKDIR%" mkdir "%WORKDIR%"
if errorlevel 1 (
    echo [ERROR] failed to create WORKDIR
    exit /b 1
)

echo [INFO] WORKDIR     = %WORKDIR%
echo [INFO] INSTALL_DIR = %INSTALL_DIR%
echo [INFO] MSYS2       = %MSYS2_SHELL%

rem ===== bash script 생성 =====
set "BUILD_SH=%WORKDIR%\build_mediainfo.sh"

> "%BUILD_SH%" echo #!/usr/bin/env bash
>>"%BUILD_SH%" echo set -e
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Updating packages..."
>>"%BUILD_SH%" echo pacman -Sy --noconfirm --needed ^
 git ^
 mingw-w64-x86_64-gcc ^
 mingw-w64-x86_64-cmake ^
 mingw-w64-x86_64-ninja ^
 mingw-w64-x86_64-zlib
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo cd "%WORKDIR_SH%"
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo if [ ! -d ZenLib ]; then
>>"%BUILD_SH%" echo ^  echo "[INFO] Cloning ZenLib..."
>>"%BUILD_SH%" echo ^  git clone https://github.com/MediaArea/ZenLib.git
>>"%BUILD_SH%" echo else
>>"%BUILD_SH%" echo ^  echo "[INFO] Updating ZenLib..."
>>"%BUILD_SH%" echo ^  cd ZenLib ^&^& git pull --ff-only ^&^& cd ..
>>"%BUILD_SH%" echo fi
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo if [ ! -d MediaInfoLib ]; then
>>"%BUILD_SH%" echo ^  echo "[INFO] Cloning MediaInfoLib..."
>>"%BUILD_SH%" echo ^  git clone https://github.com/MediaArea/MediaInfoLib.git
>>"%BUILD_SH%" echo else
>>"%BUILD_SH%" echo ^  echo "[INFO] Updating MediaInfoLib..."
>>"%BUILD_SH%" echo ^  cd MediaInfoLib ^&^& git pull --ff-only ^&^& cd ..
>>"%BUILD_SH%" echo fi
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Removing old build/install caches..."
>>"%BUILD_SH%" echo rm -rf ZenLib/build-mingw64
>>"%BUILD_SH%" echo rm -rf MediaInfoLib/build-mingw64
>>"%BUILD_SH%" echo rm -rf "%INSTALL_DIR_SH%"
>>"%BUILD_SH%" echo mkdir -p "%INSTALL_DIR_SH%"
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Configuring ZenLib..."
>>"%BUILD_SH%" echo cmake -S ZenLib/Project/CMake -B ZenLib/build-mingw64 -G Ninja ^
 -DCMAKE_BUILD_TYPE=Release ^
 -DBUILD_SHARED_LIBS=OFF ^
 -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR_SH%"
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Building ZenLib..."
>>"%BUILD_SH%" echo cmake --build ZenLib/build-mingw64 --parallel
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Installing ZenLib..."
>>"%BUILD_SH%" echo cmake --install ZenLib/build-mingw64
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Configuring MediaInfoLib..."
>>"%BUILD_SH%" echo cmake -S MediaInfoLib/Project/CMake -B MediaInfoLib/build-mingw64 -G Ninja ^
 -DCMAKE_BUILD_TYPE=Release ^
 -DBUILD_SHARED_LIBS=ON ^
 -DBUILD_ZENLIB=OFF ^
 -DBUILD_ZLIB=OFF ^
 -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR_SH%" ^
 -DCMAKE_PREFIX_PATH="%INSTALL_DIR_SH%;/mingw64"
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] MediaInfoLib CMake cache:"
>>"%BUILD_SH%" echo grep -E "BUILD_SHARED_LIBS:BOOL=^|BUILD_ZENLIB:BOOL=^|BUILD_ZLIB:BOOL=^|CMAKE_INSTALL_PREFIX:" MediaInfoLib/build-mingw64/CMakeCache.txt ^| cat
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Building MediaInfoLib..."
>>"%BUILD_SH%" echo cmake --build MediaInfoLib/build-mingw64 --parallel --verbose
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Installing MediaInfoLib..."
>>"%BUILD_SH%" echo cmake --install MediaInfoLib/build-mingw64
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Searching build tree for DLL/import libs..."
>>"%BUILD_SH%" echo find MediaInfoLib/build-mingw64 -type f \( -name "*.dll" -o -name "*.dll.a" -o -name "*.a" \) ^| sort
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Searching install tree for DLL/import libs..."
>>"%BUILD_SH%" echo find "%INSTALL_DIR_SH%" -type f \( -name "*.dll" -o -name "*.dll.a" -o -name "*.a" \) ^| sort
>>"%BUILD_SH%" echo.
>>"%BUILD_SH%" echo echo "[INFO] Done."

if errorlevel 1 (
    echo [ERROR] failed to create bash script
    exit /b 1
)

echo [INFO] Running MSYS2 MinGW64 shell...
call "%MSYS2_SHELL%" -mingw64 -defterm -no-start -here -c "/usr/bin/bash \"%WORKDIR_SH%/build_mediainfo.sh\""
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

echo [OK] build finished
echo [OK] output: %INSTALL_DIR%
exit /b 0