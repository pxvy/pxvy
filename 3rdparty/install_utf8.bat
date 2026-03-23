@echo off
chcp 65001 >nul

powershell -Command "Write-Host '██╗░░░██╗████████╗███████╗░█████╗░' -ForegroundColor Cyan;Write-Host '██║░░░██║╚══██╔══╝██╔════╝██╔══██╗' -ForegroundColor Cyan;Write-Host '██║░░░██║░░░██║░░░█████╗░░╚█████╔╝' -ForegroundColor Cyan;Write-Host '██║░░░██║░░░██║░░░██╔══╝░░██╔══██╗' -ForegroundColor Cyan;Write-Host '╚██████╔╝░░░██║░░░██║░░░░░╚█████╔╝' -ForegroundColor Cyan;Write-Host '░╚═════╝░░░░╚═╝░░░╚═╝░░░░░░╚════╝░' -ForegroundColor Cyan;"


setlocal

if not exist ".\include" mkdir ".\include"

curl -L "https://raw.githubusercontent.com/sheredom/utf8.h/refs/heads/master/utf8.h" -o ".\include\utf8.h"


echo Done.
endlocal

pause