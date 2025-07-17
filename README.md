# SurroundView3D

A real-time 3D surround view system for automotive applications using OpenGL and OpenCV.

## Features

- Real-time 3D visualization of surround view from multiple cameras
- Support for front, back, left, and right camera inputs
- OpenGL-based 3D rendering with customizable car models
- Image processing pipeline for camera calibration and homography
- Compute shader support for GPU-accelerated image processing

## Dependencies

- OpenCV 4.x
- OpenGL
- GLFW 3.x
- GLEW
- GLM
- Assimp (optional, for GLB model loading)

## Build Instructions

### Prerequisites
- Visual Studio 2019 or later with C++ support
- vcpkg package manager installed at `C:\vcpkg`
- CMake 3.12 or later

### Building
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Running
```bash
cd build
./Release/SurroundView3D.exe
```

## Project Structure

- `src/` - Source code files
- `include/` - Header files  
- `shaders/` - OpenGL shaders (vertex, fragment, compute)
- `assets/` - Camera images and 3D models
- `cmake/` - CMake configuration files

## Camera Setup

Place camera images in the following directories:
- `assets/front/` - Front camera images
- `assets/back/` - Back camera images  
- `assets/left/` - Left camera images
- `assets/right/` - Right camera images

Images should be numbered sequentially (e.g., 1851.png, 1852.png, etc.)
