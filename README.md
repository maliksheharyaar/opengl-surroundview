# SurroundView3D

A real-time 3D surround view system for automotive applications using OpenGL and OpenCV. This application processes multi-camera fisheye images to create a seamless bird's-eye view around a vehicle with proper calibration and undistortion.

![SurroundView3D Demo](assets/Car_render.gif)

*Real-time 3D surround view rendering with fisheye camera undistortion and car model visualization*

## Features

- **Real-time 3D Visualization**: Interactive top-down surround view with zoom controls
- **Multi-Camera Support**: Processes front, back, left, and right camera feeds simultaneously
- **Advanced Image Processing**: 
  - Fisheye lens undistortion using camera intrinsics
  - Automatic cropping to remove vehicle frame
  - Unified calibration system with YAML configuration
- **3D Car Model Rendering**: GLB model support with material colors and lighting
- **Camera Calibration**: 
  - Intrinsic parameters from `camera_intrinsics.yml`
  - Extrinsic parameters from `camera_extrinsics.csv`
- **Optimized OpenGL Pipeline**: Hardware-accelerated rendering with selective lighting

## Dependencies

- **OpenCV 4.x** - Image processing and camera calibration
- **OpenGL 4.5+** - 3D rendering and shader support
- **GLFW 3.x** - Window management and input handling
- **GLEW** - OpenGL extension loading
- **GLM** - Mathematics library for graphics
- **Assimp** - 3D model loading (GLB format support)

## Build Instructions

### Prerequisites
- Visual Studio 2019 or later with C++ support
- vcpkg package manager installed at `C:\vcpkg`
- CMake 3.12 or later

### Building
```bash
# Clone the repository
git clone https://github.com/maliksheharyaar/opengl-surroundview.git
cd opengl-surroundview

# Create build directory
mkdir build
cd build

# Generate build files
cmake ..

# Build the project
cmake --build . --config Release
```

### Running
```bash
cd build/Release
./SurroundView3D.exe
```

## Configuration

### Camera Calibration
- **Intrinsics**: Edit `camera_intrinsics.yml` with your camera parameters (K matrix, distortion coefficients, xi parameter)
- **Extrinsics**: Update `camera_extrinsics.csv` with camera positions and rotations relative to vehicle center

### Controls
- **Mouse Scroll**: Zoom in/out (FOV range: 10° - 150°)
- **View**: Fixed top-down perspective, camera rotated 180° for proper orientation

## Project Structure

```
├── src/                    # Source code
│   ├── main.cpp           # Application entry point
│   ├── Renderer3D.cpp     # 3D rendering engine
│   ├── ImageProcessor.cpp # Camera calibration and image processing
│   ├── Camera.cpp         # Camera controls and transformations
│   ├── Shader.cpp         # OpenGL shader management
│   ├── Mesh.cpp          # 3D mesh generation
│   └── Model.cpp         # GLB model loading
├── include/               # Header files
├── shaders/              # OpenGL shaders
│   ├── vertex.glsl       # Vertex shader
│   └── fragment.glsl     # Fragment shader with selective lighting
├── assets/               # Camera images and 3D models
│   ├── front/           # Front camera images
│   ├── back/            # Back camera images  
│   ├── left/            # Left camera images
│   ├── right/           # Right camera images
│   └── model.glb        # 3D car model
├── camera_intrinsics.yml # Camera calibration parameters
├── camera_extrinsics.csv # Camera position/rotation data
└── cmake/               # CMake configuration
```

## Technical Details

### OpenGL vs 2D Python Implementation Differences

**Important Note**: This OpenGL-based surround view implementation differs significantly from traditional 2D Python surround view projects, even though both use OpenCV:

- **Coordinate Systems**: 
  - **2D Python**: Works in image/pixel coordinates (Y-axis points down, origin at top-left)
  - **OpenGL 3D**: Uses world coordinates (Y-axis points up, origin at center, right-handed system)
  - **Stitching Approach**: Direct pixel manipulation vs 3D texture mapping on geometric planes

- **Processing Pipeline**:
  - **2D Python**: Direct image stitching using homography matrices and pixel-level blending
  - **OpenGL 3D**: 3D scene rendering with texture-mapped geometry and hardware acceleration

- **Transformation Matrices**: 
  - Matrix calculations must account for coordinate system flipping (Y-axis inversion)
  - 3D perspective projections vs 2D homography transformations

### CPU vs GPU Task Distribution

#### CPU-Based Operations (OpenCV + C++)
- **Image Loading**: File I/O and initial image decoding
- **Fisheye Undistortion**: OpenCV `remap()` operations for lens correction
- **Image Preprocessing**: 
  - Color space conversion (BGR → RGB)
  - Image rotation (90°, 180° transformations)
  - Cropping to remove vehicle frame
- **Layout Composition**: Stitching multiple camera views into unified surround view
- **Camera Calibration**: YAML/CSV file parsing and parameter loading
- **Memory Management**: Image buffer allocation and data transfer preparation

#### GPU-Based Operations (OpenGL Shaders)
- **3D Rendering Pipeline**: Vertex and fragment shader execution
- **Texture Mapping**: Hardware-accelerated texture sampling and filtering
- **Geometric Transformations**: Matrix operations for 3D model positioning
- **Lighting Calculations**: 
  - Ambient and directional lighting for car model
  - Selective lighting (unlit rendering for surround view plane)
- **Real-time Display**: Frame buffer operations and screen rendering
- **Perspective Projection**: 3D to 2D coordinate transformation

### Image Processing Pipeline
1. **Fisheye Undistortion**: Uses camera intrinsic parameters (K matrix, distortion coefficients, xi) to correct lens distortion
2. **Cropping**: Automatically removes vehicle frame from images using optimized crop regions
3. **Layout Composition**: Combines four camera views into a unified surround view with proper aspect ratios
4. **Texture Mapping**: Maps processed images onto a 3D plane mesh for seamless rendering

### Rendering Pipeline
- **Selective Lighting**: Car model receives full ambient and directional lighting, while surround view plane uses unlit rendering
- **Material Support**: GLB models display embedded textures and material colors
- **Zoom Controls**: Smooth FOV transitions with maintained top-down perspective
- **Aspect Ratio Preservation**: Corrected plane mesh ensures proper image proportions

### Calibration Format

**camera_intrinsics.yml**:
```yaml
front:
  K: [fx, 0, cx, 0, fy, cy, 0, 0, 1]  # Camera matrix
  D: [k1, k2, k3, k4]                  # Distortion coefficients
  xi: 0.5                              # Xi parameter
# Similar entries for back, left, right
```

**camera_extrinsics.csv**:
```csv
camera,x,y,z,roll,pitch,yaw,fx,fy,cx,cy,k1,k2,p1,p2
front,0.0,2.5,1.5,0.0,0.0,0.0,400,400,320,240,-0.2,0.1,0,0
# Positions relative to vehicle center (rear axle)
```

## Performance Notes

### CPU Performance
- **OpenCV Operations**: Single-threaded fisheye undistortion and image processing
- **Memory Bandwidth**: CPU-GPU data transfer for texture updates
- **I/O Operations**: File loading and camera parameter parsing
- **Image Composition**: Multi-camera view stitching and layout arrangement

### GPU Performance  
- **Hardware Acceleration**: Parallel processing of vertices and fragments
- **Texture Memory**: Optimized texture caching and sampling
- **Shader Execution**: Simultaneous processing of multiple pixels/vertices
- **Real-time Rendering**: 30+ FPS with multiple camera inputs

### Architecture Benefits
- **Hybrid Processing**: CPU handles complex image algorithms, GPU handles rendering
- **Scalable Design**: Modular pipeline allows independent optimization of CPU/GPU tasks
- **Memory Efficiency**: Minimized CPU-GPU transfers through optimized texture management

## Troubleshooting

### Common Issues
- **Black car model**: Check lighting settings in fragment shader
- **Distorted images**: Verify camera intrinsic parameters in YAML file
- **Missing textures**: Ensure GLB model contains embedded materials
- **Build errors**: Check vcpkg installation and OpenCV/OpenGL dependencies
- **Coordinate system issues**: Remember Y-axis inversion between OpenCV (down) and OpenGL (up)
- **Performance bottlenecks**: Monitor CPU usage for image processing vs GPU usage for rendering

### Migration from 2D Python Surround View
If adapting from a 2D Python implementation:
- **Matrix Transformations**: Invert Y-coordinates and adjust homography matrices
- **Image Coordinates**: Convert from pixel-based to normalized texture coordinates
- **Stitching Logic**: Replace direct pixel manipulation with 3D texture mapping
- **Calibration Data**: May need to adjust camera parameters for 3D coordinate system

### System Requirements
- **GPU**: OpenGL 4.5+ compatible graphics card
- **RAM**: Minimum 4GB, recommended 8GB+
- **Storage**: ~2GB for full project with images
- **OS**: Windows 10/11 (tested), Linux support via CMake
