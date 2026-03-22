@echo off
setlocal

python -m nuitka smi_to_srt.py ^
  --onefile ^
  --lto=yes ^
  --follow-imports ^
  --remove-output ^
  --assume-yes-for-downloads ^
  --enable-plugin=upx ^
  --plugin-enable=anti-bloat ^
  --noinclude-pytest-mode=nofollow ^
  --noinclude-setuptools-mode=nofollow ^
  --noinclude-unittest-mode=nofollow ^
  --noinclude-IPython-mode=nofollow ^
  --noinclude-dask-mode=nofollow ^
  --windows-icon-from-ico=python.ico

if errorlevel 1 exit /b 1

for %%D in (
    "..\cmake-build-debug"
    "..\cmake-build-debug-llvm"
    "..\cmake-build-release"
    "..\cmake-build-release-llvm"
) do (
    if exist "%%~D" (
        copy /Y "smi_to_srt.exe" "%%~D\smi_to_srt.exe"
    )
)

endlocal