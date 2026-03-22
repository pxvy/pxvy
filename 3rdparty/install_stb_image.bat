@echo off
setlocal

if not exist ".\include" mkdir ".\include"

curl -L "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_write.h" -o ".\include\stb_image_write.h"
curl -L "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_resize2.h" -o ".\include\stb_image_resize2.h"
curl -L "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h" -o ".\include\stb_image.h"

echo Done.
endlocal

pause