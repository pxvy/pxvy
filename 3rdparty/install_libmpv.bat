@echo off

chcp 65001 >nul
powershell -Command "Write-Host '██╗░░░░░██╗██████╗░███╗░░░███╗██████╗░██╗░░░██╗' -ForegroundColor Cyan;Write-Host '██║░░░░░██║██╔══██╗████╗░████║██╔══██╗██║░░░██║' -ForegroundColor Cyan;Write-Host '██║░░░░░██║██████╦╝██╔████╔██║██████╔╝╚██╗░██╔╝' -ForegroundColor Cyan;Write-Host '██║░░░░░██║██╔══██╗██║╚██╔╝██║██╔═══╝░░╚████╔╝░' -ForegroundColor Cyan;Write-Host '███████╗██║██████╦╝██║░╚═╝░██║██║░░░░░░░╚██╔╝░░' -ForegroundColor Cyan;Write-Host '╚══════╝╚═╝╚═════╝░╚═╝░░░░░╚═╝╚═╝░░░░░░░░╚═╝░░░' -ForegroundColor Cyan;"

setlocal

set "URL=https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20260307/mpv-dev-x86_64-v3-20260307-git-f9190e5.7z"
set "ARCHIVE_FILE=mpv-dev.7z"
set "EXTRACT_DIR=mpv_temp"

if not exist "include" mkdir "include"
if not exist "lib" mkdir "lib"
if not exist "bin" mkdir "bin"

echo [+] mpv Downloading...
curl -L "%URL%" -o "%ARCHIVE_FILE%"

echo [+] Decompressing...
if exist "%EXTRACT_DIR%" rd /s /q "%EXTRACT_DIR%"
7z x "%ARCHIVE_FILE%" -o"%EXTRACT_DIR%" -y

echo [+] Move...
pushd "%EXTRACT_DIR%"

if exist "include" (
    xcopy "include" "..\include\" /E /I /Y
)

for /r %%f in (libmpv.dll.a) do move /y "%%f" "..\lib\"
for /r %%f in (libmpv-2.dll) do move /y "%%f" "..\bin\"

popd

echo [+] Cleanup...
if exist "%ARCHIVE_FILE%" del /q "%ARCHIVE_FILE%"
if exist "%EXTRACT_DIR%" rd /s /q "%EXTRACT_DIR%"

echo Done.
pause

exit /b 0