@echo off
setlocal

mkdir TEMP_BUILD
cd TEMP_BUILD

git clone https://github.com/microsoft/mimalloc
cd mimalloc

mkdir build
cd build

cmake .. -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DMI_BUILD_SHARED=OFF ^
  -DMI_BUILD_TESTS=OFF

cmake --build . --config Release --parallel

if not exist "..\..\..\lib" mkdir "..\..\..\lib"
if not exist "..\..\..\include" mkdir "..\..\..\include"

copy /Y "libmimalloc.a" "..\..\..\lib\"

cd ..\include

xcopy * "..\..\..\include\" /E /I /Y

cd ..\..\..\
pwd
call :SafeRMDIR "TEMP_BUILD"
pause
endlocal


:SafeRMDIR
IF EXIST "%~1" (
    RMDIR /S /Q "%~1"
)
exit /b