@echo off
echo SurroundView3D Performance Profiler Setup and Launch
echo ===================================================

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://python.org
    pause
    exit /b 1
)

echo Python found, checking dependencies...

REM Install profiler dependencies
echo Installing profiler dependencies...
pip install -r profiler_requirements.txt

if errorlevel 1 (
    echo Error: Failed to install dependencies
    echo Please ensure you have internet connection and pip is working
    pause
    exit /b 1
)

echo Dependencies installed successfully!
echo.

REM Check if SurroundView3D.exe exists
if not exist "build\Release\SurroundView3D.exe" (
    echo Warning: SurroundView3D.exe not found in build\Release\
    echo Please build the project first using build.bat
    echo.
    echo Do you want to build it now? [Y/N]
    set /p build_choice=
    if /i "%build_choice%"=="Y" (
        call build.bat
        if errorlevel 1 (
            echo Build failed. Please check the build output.
            pause
            exit /b 1
        )
    ) else (
        echo Please build the project first, then run the profiler
        pause
        exit /b 1
    )
)

echo Launching Performance Profiler...
echo.
echo Instructions:
echo 1. Click "Launch SurroundView3D" to start the application
echo 2. Monitor real-time performance metrics in the charts
echo 3. Use "Stop Application" to end monitoring
echo.

REM Launch the profiler
python profiler.py

echo Profiler closed.
pause
