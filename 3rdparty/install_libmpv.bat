@echo off
setlocal

set "URL=https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20260307/mpv-dev-x86_64-v3-20260307-git-f9190e5.7z"
set "ARCHIVE_FILE=mpv-dev.7z"
set "EXTRACT_DIR=mpv_temp"

if not exist "include" mkdir "include"
if not exist "lib" mkdir "lib"
if not exist "bin" mkdir "bin"

echo [+] mpv Downloading...
powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%ARCHIVE_FILE%'"

echo [+] Decompressing...
if exist "%EXTRACT_DIR%" rd /s /q "%EXTRACT_DIR%"
7z x "%ARCHIVE_FILE%" -o"%EXTRACT_DIR%" -y

echo [+] Move...
pushd "%EXTRACT_DIR%"

for /r %%f in (mpv_client_api.h) do (
    if exist "%%~dpf..\libmpv.dll.a" (
        if exist "..\include\mpv" rd /s /q "..\include\mpv"
        move "%%~dpf.." "..\include\"
    )
)

for /r %%f in (libmpv.dll.a) do move /y "%%f" "..\lib\"
for /r %%f in (libmpv-2.dll) do move /y "%%f" "..\bin\"

popd

echo [+] Cleanup...
del /q "%ARCHIVE_FILE%"
rd /s /q "%EXTRACT_DIR%"

echo [!] Done.
pause