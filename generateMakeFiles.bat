@echo off
setlocal

if "%1" == "-d" (
    cmake .
) else (
    cmake .
)

endlocal