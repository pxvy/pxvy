@echo off
chcp 65001 >nul
powershell -Command "Write-Host '░██████╗░██████╗░██╗░░░░░██╗████████╗███████╗██████╗░' -ForegroundColor Cyan;Write-Host '██╔════╝██╔═══██╗██║░░░░░██║╚══██╔══╝██╔════╝╚════██╗' -ForegroundColor Cyan;Write-Host '╚█████╗░██║██╗██║██║░░░░░██║░░░██║░░░█████╗░░░█████╔╝' -ForegroundColor Cyan;Write-Host '░╚═══██╗╚██████╔╝██║░░░░░██║░░░██║░░░██╔══╝░░░╚═══██╗' -ForegroundColor Cyan;Write-Host '██████╔╝░╚═██╔═╝░███████╗██║░░░██║░░░███████╗██████╔╝' -ForegroundColor Cyan;Write-Host '╚═════╝░░░░╚═╝░░░╚══════╝╚═╝░░░╚═╝░░░╚══════╝╚═════╝░' -ForegroundColor Cyan;"


setlocal


set "URL=https://sqlite.org/2026/sqlite-amalgamation-3510300.zip"
set "ZIP_FILE=sqlite.zip"
set "EXTRACT_DIR=sqlite_temp"

if not exist "src" mkdir "src"
if not exist "include" mkdir "include"

echo [+] SQLite Downloading...
powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%ZIP_FILE%'"

echo [+] Decompressing...
if exist "%EXTRACT_DIR%" rd /s /q "%EXTRACT_DIR%"
powershell -Command "Expand-Archive -Path '%ZIP_FILE%' -DestinationPath '%EXTRACT_DIR%'"

echo [+] Move...
:: 압축 파일 내부에 폴더가 한 겹 더 있을 수 있으므로 /s 옵션으로 검색하여 복사합니다.
pushd "%EXTRACT_DIR%"
for /r %%f in (sqlite3.c) do if exist "%%f" move /y "%%f" "..\src\"
for /r %%f in (sqlite3.h) do if exist "%%f" move /y "%%f" "..\include\"
popd

echo [+] Cleanup...
del /q "%ZIP_FILE%"
rd /s /q "%EXTRACT_DIR%"

echo [!] Done.
pause