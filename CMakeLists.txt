cmake_minimum_required(VERSION 3.12)

# Set vcpkg toolchain if not already set (must be before project() call)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(SurroundView3D)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include platform-specific configuration
include(cmake/config.cmake)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

# Find optional packages
find_package(assimp CONFIG QUIET)

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Renderer3D.cpp
    src/ImageProcessor.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Mesh.cpp
)

# Conditionally add Model.cpp if assimp is found
if(assimp_FOUND)
    target_sources(${PROJECT_NAME} PRIVATE src/Model.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_ASSIMP)
    message(STATUS "Assimp found - GLB model loading enabled")
else()
    message(STATUS "Assimp not found - using simple car model only")
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    glfw
    GLEW::GLEW
    glm::glm
)

# Conditionally link assimp if found
if(assimp_FOUND)
    target_link_libraries(${PROJECT_NAME} assimp::assimp)
endif()

# Copy shaders to build directory
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
