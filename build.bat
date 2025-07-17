@echo off
echo Building SurroundView3D...

if not exist build (
    mkdir build
)

cd build
cmake ..
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build completed successfully!
echo Run the application with: .\build\Release\SurroundView3D.exe
