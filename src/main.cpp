#include "Renderer3D.h"
#include "ImageProcessor.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <thread>

int main() {
    std::cout << "Starting SurroundView3D Application..." << std::endl;

    // Initialize renderer
    Renderer3D renderer(1200, 800);
    if (!renderer.initialize()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        return -1;
    }

    // Initialize image processor
    ImageProcessor imageProcessor;
    
    // Load image sequences from all camera folders
    std::vector<std::string> frontSequence, leftSequence, rightSequence, backSequence;
    std::string frontFolder = "assets/front/";
    std::string leftFolder = "assets/left/";
    std::string rightFolder = "assets/right/";
    std::string backFolder = "assets/back/";
    
    // Debug: Print current working directory
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
    
    // Generate image paths for sequence 1851.png to 1999.png for all cameras
    for (int i = 1851; i <= 1999; ++i) {
        frontSequence.push_back(frontFolder + std::to_string(i) + ".png");
        leftSequence.push_back(leftFolder + std::to_string(i) + ".png");
        rightSequence.push_back(rightFolder + std::to_string(i) + ".png");
        backSequence.push_back(backFolder + std::to_string(i) + ".png");
    }
    
    std::cout << "Loading image sequences from all camera folders (" << frontSequence.size() << " images each)" << std::endl;
    
    // Debug: Check if all camera folders exist
    std::vector<std::string> folders = {frontFolder, leftFolder, rightFolder, backFolder};
    std::vector<std::string> folderNames = {"Front", "Left", "Right", "Back"};
    bool allFoldersExist = true;
    
    for (size_t i = 0; i < folders.size(); i++) {
        if (std::filesystem::exists(folders[i])) {
            std::cout << folderNames[i] << " folder exists." << std::endl;
        } else {
            std::cout << folderNames[i] << " folder does not exist!" << std::endl;
            allFoldersExist = false;
        }
    }
    
    // Verify at least the first images exist from all cameras
    cv::Mat fallbackImage;
    cv::Mat firstFront = imageProcessor.loadImage(frontSequence[0]);
    cv::Mat firstLeft = imageProcessor.loadImage(leftSequence[0]);
    cv::Mat firstRight = imageProcessor.loadImage(rightSequence[0]);
    cv::Mat firstBack = imageProcessor.loadImage(backSequence[0]);
    
    if (firstFront.empty() || firstLeft.empty() || firstRight.empty() || firstBack.empty() || !allFoldersExist) {
        std::cerr << "Could not load images from all cameras. Missing: ";
        if (firstFront.empty()) std::cerr << "front ";
        if (firstLeft.empty()) std::cerr << "left ";
        if (firstRight.empty()) std::cerr << "right ";
        if (firstBack.empty()) std::cerr << "back ";
        std::cerr << std::endl;
        
        std::cout << "Trying to load sample image instead..." << std::endl;
        
        // Try to load sample image as fallback
        fallbackImage = imageProcessor.loadImage("assets/sample_image.png");
        if (fallbackImage.empty()) {
            fallbackImage = imageProcessor.loadImage("assets/sample_image.jpg");
        }
        
        if (fallbackImage.empty()) {
            std::cout << "Generating test pattern instead..." << std::endl;
            fallbackImage = imageProcessor.generateTestGrid(800, 600);
        } else {
            std::cout << "Using sample image as fallback." << std::endl;
        }
        // Clear sequences to use fallback image
        frontSequence.clear();
        leftSequence.clear();
        rightSequence.clear();
        backSequence.clear();
        // Set initial texture to fallback
        renderer.updateTexture(fallbackImage);
    } else {
        std::cout << "All camera image sequences loaded successfully!" << std::endl;
        std::cout << "Image size: " << firstFront.cols << "x" << firstFront.rows << std::endl;
        const double frameRate = 30.0; // 30 FPS  
        std::cout << "Starting surround view stream with " << frontSequence.size() << " frames at " << frameRate << " FPS" << std::endl;
        std::cout << "Applying full pipeline: fisheye undistortion + homography transformation..." << std::endl;
        std::cout << "Using parallel processing with " << std::thread::hardware_concurrency() << " threads" << std::endl;
        
        // Create surround view from all four cameras using parallel processing
        cv::Mat surroundView = imageProcessor.createSurroundViewParallel(firstFront, firstLeft, firstRight, firstBack);
        if (!surroundView.empty()) {
            renderer.updateTexture(surroundView);
        } else {
            std::cerr << "Surround view creation failed! Using front camera..." << std::endl;
            cv::Mat processedFront = imageProcessor.processFullPipeline(firstFront);
            renderer.updateTexture(processedFront.empty() ? firstFront : processedFront);
        }
    }

    std::cout << "Rendering video stream. Use mouse scroll wheel to zoom in/out." << std::endl;
    std::cout << "Press ESC to exit." << std::endl;

    // Video stream variables
    size_t currentImageIndex = 0;
    const double frameRate = 30.0; // 30 FPS
    auto lastFrameTime = std::chrono::steady_clock::now();
    const auto frameDuration = std::chrono::duration<double>(1.0 / frameRate);

    // Main render loop
    while (!renderer.shouldClose()) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = currentTime - lastFrameTime;
        
        // Update frame if enough time has passed and we have sequences
        if (elapsed >= frameDuration && !frontSequence.empty()) {
            // Load current images from all cameras
            cv::Mat currentFront = imageProcessor.loadImage(frontSequence[currentImageIndex]);
            cv::Mat currentLeft = imageProcessor.loadImage(leftSequence[currentImageIndex]);
            cv::Mat currentRight = imageProcessor.loadImage(rightSequence[currentImageIndex]);
            cv::Mat currentBack = imageProcessor.loadImage(backSequence[currentImageIndex]);
            
            if (!currentFront.empty() && !currentLeft.empty() && !currentRight.empty() && !currentBack.empty()) {
                // Create surround view from all four cameras using parallel processing
                cv::Mat surroundView = imageProcessor.createSurroundViewParallel(currentFront, currentLeft, currentRight, currentBack);
                
                if (!surroundView.empty()) {
                    renderer.updateTexture(surroundView);
                    std::cout << "Frame " << currentImageIndex + 1 << "/" << frontSequence.size() << " - Surround view created" << std::endl;
                } else {
                    std::cerr << "Surround view creation failed! Using front camera only..." << std::endl;
                    cv::Mat processedFront = imageProcessor.processFullPipeline(currentFront);
                    renderer.updateTexture(processedFront.empty() ? currentFront : processedFront);
                    std::cout << "Fallback: Front camera only" << std::endl;
                }
            }
            
            // Move to next image (loop back to start when we reach the end)
            currentImageIndex = (currentImageIndex + 1) % frontSequence.size();
            lastFrameTime = currentTime;
            
            if (currentImageIndex == 0) {
                std::cout << "Surround view loop completed, restarting..." << std::endl;
            }
        }

        renderer.pollEvents();
        renderer.updateCamera(0.016f); // Assuming ~60 FPS
        renderer.render();
        renderer.swapBuffers();
    }

    renderer.cleanup();
    std::cout << "Application closed successfully." << std::endl;
    return 0;
}
