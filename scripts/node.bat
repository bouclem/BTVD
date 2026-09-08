@echo off
title BitVoid Node
echo ================================
echo   BitVoid Node
echo ================================
echo.

where bitvoid-core >nul 2>nul
if %errorlevel% neq 0 (
    echo bitvoid-core not found. Looking in current directory...
    if exist bitvoid-core.exe (
        set PATH=%CD%;%PATH%
    ) else (
        echo Please download bitvoid-core.exe and put it in this folder.
        pause
        exit /b 1
    )
)

bitvoid-core node %*
pause
