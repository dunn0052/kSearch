@echo off
setlocal

if "%1" == "-d" (
    .\generateMakefiles.bat -d
    cmake --build . --config Debug -j 12
) else (
    .\generateMakefiles.bat
    cmake --build . --config Release -j 12
)

endlocal