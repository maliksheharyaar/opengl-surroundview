#include "ImageProcessor.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <future>
#include <chrono>

ImageProcessor::ImageProcessor() : m_threadPoolInitialized(false), m_numThreads(0) {
    // Load camera parameters from YAML files on initialization
    loadCameraParameters("front");
    loadCameraParameters("left");
    loadCameraParameters("right");
    loadCameraParameters("back");
    
    // Load extrinsic parameters from CSV file
    loadExtrinsicParameters();
    
    // Initialize thread pool
    initializeThreadPool();
}

ImageProcessor::~ImageProcessor() {
    shutdownThreadPool();
}

cv::Mat ImageProcessor::loadImage(const std::string& filepath) {
    std::cout << "Attempting to load image: " << filepath << std::endl;
    cv::Mat image = cv::imread(filepath, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cout << "Could not load image: " << filepath << std::endl;
        return cv::Mat();
    }
    std::cout << "Raw image loaded - Size: " << image.cols << "x" << image.rows 
              << ", Channels: " << image.channels() << ", Type: " << image.type() << std::endl;
    return preprocessImage(image);
}

cv::Mat ImageProcessor::createBirdEyeView(const cv::Mat& input) {
    if (input.empty()) return input;

    // Get perspective transformation matrix
    cv::Mat transformMatrix = getDefaultPerspectiveMatrix(input.cols, input.rows);
    
    // Apply perspective transformation
    return applyPerspectiveTransform(input, transformMatrix);
}

cv::Mat ImageProcessor::applyPerspectiveTransform(const cv::Mat& input, const cv::Mat& transformMatrix) {
    cv::Mat output;
    cv::warpPerspective(input, output, transformMatrix, cv::Size(input.cols, input.rows));
    return output;
}

cv::Mat ImageProcessor::generateTestGrid(int width, int height) {
    cv::Mat grid = cv::Mat::zeros(height, width, CV_8UC3);
    
    // Draw grid lines
    int gridSize = 50;
    cv::Scalar lineColor(100, 100, 100); // Gray lines
    cv::Scalar highlightColor(255, 255, 255); // White lines for major grid
    
    // Vertical lines
    for (int x = 0; x < width; x += gridSize) {
        cv::Scalar color = (x % (gridSize * 4) == 0) ? highlightColor : lineColor;
        cv::line(grid, cv::Point(x, 0), cv::Point(x, height), color, 1);
    }
    
    // Horizontal lines
    for (int y = 0; y < height; y += gridSize) {
        cv::Scalar color = (y % (gridSize * 4) == 0) ? highlightColor : lineColor;
        cv::line(grid, cv::Point(0, y), cv::Point(width, y), color, 1);
    }
    
    // Add center cross
    cv::line(grid, cv::Point(width/2 - 20, height/2), cv::Point(width/2 + 20, height/2), cv::Scalar(0, 255, 0), 3);
    cv::line(grid, cv::Point(width/2, height/2 - 20), cv::Point(width/2, height/2 + 20), cv::Scalar(0, 255, 0), 3);
    
    // Add some colored squares for reference
    cv::rectangle(grid, cv::Point(width/4 - 25, height/4 - 25), cv::Point(width/4 + 25, height/4 + 25), cv::Scalar(0, 0, 255), -1);
    cv::rectangle(grid, cv::Point(3*width/4 - 25, height/4 - 25), cv::Point(3*width/4 + 25, height/4 + 25), cv::Scalar(255, 0, 0), -1);
    cv::rectangle(grid, cv::Point(width/4 - 25, 3*height/4 - 25), cv::Point(width/4 + 25, 3*height/4 + 25), cv::Scalar(0, 255, 255), -1);
    cv::rectangle(grid, cv::Point(3*width/4 - 25, 3*height/4 - 25), cv::Point(3*width/4 + 25, 3*height/4 + 25), cv::Scalar(255, 0, 255), -1);
    
    // Add text
    cv::putText(grid, "Test Pattern - Bird's Eye View", cv::Point(20, 30), 
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
    
    return grid;
}

cv::Mat ImageProcessor::generateCheckerboard(int width, int height, int squareSize) {
    cv::Mat checkerboard = cv::Mat::zeros(height, width, CV_8UC3);
    
    for (int y = 0; y < height; y += squareSize) {
        for (int x = 0; x < width; x += squareSize) {
            if (((x / squareSize) + (y / squareSize)) % 2 == 0) {
                cv::rectangle(checkerboard, 
                            cv::Point(x, y), 
                            cv::Point(std::min(x + squareSize, width), std::min(y + squareSize, height)), 
                            cv::Scalar(255, 255, 255), -1);
            }
        }
    }
    
    return checkerboard;
}

cv::Mat ImageProcessor::preprocessImage(const cv::Mat& input) {
    cv::Mat output;
    
    // Ensure the image is in the correct format
    if (input.channels() == 4) {
        cv::cvtColor(input, output, cv::COLOR_BGRA2BGR);
    } else if (input.channels() == 1) {
        cv::cvtColor(input, output, cv::COLOR_GRAY2BGR);
    } else {
        output = input.clone();
    }
    
    // Resize if too large (for performance)
    if (output.cols > 1920 || output.rows > 1080) {
        double scale = std::min(1920.0 / output.cols, 1080.0 / output.rows);
        cv::resize(output, output, cv::Size(), scale, scale, cv::INTER_LINEAR);
    }
    
    return output;
}

void ImageProcessor::saveImage(const cv::Mat& image, const std::string& filepath) {
    cv::imwrite(filepath, image);
}

cv::Mat ImageProcessor::getDefaultPerspectiveMatrix(int width, int height) {
    // Define source points (original image corners) - less aggressive perspective
    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(0, height),                    // Bottom-left
        cv::Point2f(width, height),                // Bottom-right  
        cv::Point2f(width * 0.15f, height * 0.3f), // Top-right (less aggressive, focus more on road)
        cv::Point2f(width * 0.85f, height * 0.3f)  // Top-left (less aggressive, focus more on road)
    };
    
    // Define destination points (bird's eye view) - less stretching
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(width * 0.25f, height),        // Bottom-left (less stretching)
        cv::Point2f(width * 0.75f, height),        // Bottom-right (less stretching)
        cv::Point2f(width * 0.75f, height * 0.2f), // Top-right (focus on road area)
        cv::Point2f(width * 0.25f, height * 0.2f)  // Top-left (focus on road area)
    };
    
    return cv::getPerspectiveTransform(srcPoints, dstPoints);
}

cv::Mat ImageProcessor::applyHomography(const cv::Mat& input) {
    if (input.empty()) return input;
    
    // Get homography matrix for front camera
    cv::Mat homographyMatrix = getFrontCameraHomography(input.cols, input.rows);
    
    // Apply homography transformation
    return applyPerspectiveTransform(input, homographyMatrix);
}

cv::Mat ImageProcessor::getFrontCameraHomography(int width, int height) {
    // Define source points (original image corners with perspective distortion)  
    // Less aggressive settings to show more road and less sky
    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(width * 0.05f, height * 0.75f),   // Bottom-left (closer to road surface)
        cv::Point2f(width * 0.95f, height * 0.75f),   // Bottom-right (closer to road surface)  
        cv::Point2f(width * 0.35f, height * 0.45f),   // Top-left (focus on road, not sky)
        cv::Point2f(width * 0.65f, height * 0.45f)    // Top-right (focus on road, not sky)
    };
    
    // Define destination points (corrected perspective - less stretching)
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(width * 0.3f, height * 0.85f),    // Bottom-left (less stretching)
        cv::Point2f(width * 0.7f, height * 0.85f),    // Bottom-right (less stretching)
        cv::Point2f(width * 0.3f, height * 0.4f),     // Top-left (focus on road area)
        cv::Point2f(width * 0.7f, height * 0.4f)      // Top-right (focus on road area)
    };
    
    return cv::getPerspectiveTransform(srcPoints, dstPoints);
}

cv::Mat ImageProcessor::undistortFisheye(const cv::Mat& input) {
    if (input.empty()) return input;
    
    std::cout << "DEBUG: Fisheye undistortion - Input size: " << input.cols << "x" << input.rows << std::endl;
    
    cv::Mat cameraMatrix, distCoeffs;
    getFisheyeCameraParams(cameraMatrix, distCoeffs, input.size());
    
    std::cout << "DEBUG: Camera matrix: " << cameraMatrix << std::endl;
    std::cout << "DEBUG: Distortion coeffs: " << distCoeffs.t() << std::endl;
    
    cv::Mat undistorted;
    try {
        // Use a more conservative approach - keep original camera matrix
        // This preserves more of the image content
        cv::fisheye::undistortImage(input, undistorted, cameraMatrix, distCoeffs, cameraMatrix);
        
        // Crop to remove car frame/hood from bottom of image
        // Adjust this percentage if needed: 0.7 = keep top 70%, 0.8 = keep top 80%
        double keepRatio = 0.65; // Keep top 65%, remove bottom 35% (more aggressive cropping)
        int keepHeight = static_cast<int>(undistorted.rows * keepRatio);
        int fullWidth = undistorted.cols;
        int startY = 0; // Start from top of image
        int startX = 0; // Start from left edge
        
        // Define crop rectangle: take top portion, discard bottom portion
        cv::Rect cropRect(startX, startY, fullWidth, keepHeight);
        cv::Mat cropped = undistorted(cropRect).clone();
        
        std::cout << "DEBUG: Cropping - Original: " << undistorted.rows << "x" << undistorted.cols 
                  << " -> Cropped: " << keepHeight << "x" << fullWidth 
                  << " (removed bottom " << (undistorted.rows - keepHeight) << " pixels, " 
                  << (100 * (1 - keepRatio)) << "% of image)" << std::endl;
        
        // Resize back to original dimensions to maintain consistency
        cv::Mat final;
        cv::resize(cropped, final, undistorted.size(), 0, 0, cv::INTER_LINEAR);
        
        // Check if the result is valid (not mostly black)
        cv::Scalar meanVal = cv::mean(final);
        double totalMean = (meanVal[0] + meanVal[1] + meanVal[2]) / 3.0;
        
        if (totalMean < 5.0) {
            std::cerr << "WARNING: Undistorted image appears to be mostly black (mean=" << totalMean << "), using original" << std::endl;
            return input;
        }
        
        std::cout << "DEBUG: Fisheye undistortion + cropping successful - Removed bottom " << (undistorted.rows - keepHeight) << " pixels" << std::endl;
        return final;
        
    } catch (const cv::Exception& e) {
        std::cerr << "ERROR: Fisheye undistortion failed: " << e.what() << std::endl;
        return input; // Return original image if undistortion fails
    }
}

void ImageProcessor::getFisheyeCameraParams(cv::Mat& cameraMatrix, cv::Mat& distCoeffs, cv::Size imageSize) {
    // Very mild fisheye camera parameters for automotive front camera
    // Your images appear to have minimal fisheye distortion
    
    double fx = imageSize.width * 1.0;   // Focal length X (no scaling)
    double fy = imageSize.height * 1.0;  // Focal length Y (no scaling)
    double cx = imageSize.width * 0.5;   // Principal point X (center)
    double cy = imageSize.height * 0.5;  // Principal point Y (center)
    
    // Camera intrinsic matrix
    cameraMatrix = (cv::Mat_<double>(3, 3) << 
        fx, 0,  cx,
        0,  fy, cy,
        0,  0,  1);
    
    // Very mild fisheye distortion coefficients [k1, k2, k3, k4]
    // Minimal correction to preserve image content
    distCoeffs = (cv::Mat_<double>(4, 1) << 
        -0.01,   // k1 - very mild barrel distortion
         0.0,    // k2 - disabled
         0.0,    // k3 - disabled
         0.0     // k4 - disabled
    );
}

cv::Mat ImageProcessor::processFullPipeline(const cv::Mat& input) {
    if (input.empty()) return input;
    
    // For now, disable homography and only apply fisheye undistortion
    // Step 1: Undistort fisheye lens distortion using YAML parameters
    // We'll determine the camera based on the processing context
    cv::Mat undistorted = undistortFisheye(input);
    
    // Step 2: Skip homography for now (as requested)
    // cv::Mat final = applyHomography(undistorted);
    
    return undistorted;
}

cv::Mat ImageProcessor::createSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    if (front.empty() || left.empty() || right.empty() || back.empty()) {
        std::cerr << "One or more camera images are empty!" << std::endl;
        return cv::Mat();
    }
    
    // Process each camera image with its specific YAML parameters (no homography for now)
    cv::Mat processedFront = undistortWithYAMLParams(front, "front");
    cv::Mat processedLeft = undistortWithYAMLParams(left, "left");
    cv::Mat processedRight = undistortWithYAMLParams(right, "right");
    cv::Mat processedBack = undistortWithYAMLParams(back, "back");
    
    // Apply rotations as requested:
    // - Left: 90 degrees counter-clockwise
    // - Right: 90 degrees clockwise  
    // - Back: 180 degrees
    processedLeft = rotateImage90CounterClockwise(processedLeft);
    processedRight = rotateImage90Clockwise(processedRight);
    processedBack = rotateImage180(processedBack);
    
    // After rotation, left and right images will have different dimensions
    // Use rectangular sections that preserve aspect ratio better and expand vertically
    int baseWidth = 720;   // Base width for each section
    int baseHeight = 640;  // Increased base height for each section (was 540)
    
    // Create different sized sections for better proportion:
    // Front/Back: wider sections (landscape orientation) with more vertical space
    // Left/Right: taller sections (portrait orientation after rotation)
    int frontBackWidth = baseWidth + 120;   // 840 pixels wide
    int frontBackHeight = baseHeight + 80;  // 720 pixels tall (increased from 540)
    int leftRightWidth = baseWidth;         // 720 pixels wide  
    int leftRightHeight = baseHeight + 160; // 800 pixels tall (increased from 660)
    
    // Resize images to their appropriate dimensions
    cv::resize(processedFront, processedFront, cv::Size(frontBackWidth, frontBackHeight));
    cv::resize(processedLeft, processedLeft, cv::Size(leftRightWidth, leftRightHeight));
    cv::resize(processedRight, processedRight, cv::Size(leftRightWidth, leftRightHeight));
    cv::resize(processedBack, processedBack, cv::Size(frontBackWidth, frontBackHeight));
    
    // Calculate surround view dimensions based on the new layout
    int surroundWidth = frontBackWidth + leftRightWidth * 2;  // 840 + 720 + 720 = 2280
    int surroundHeight = frontBackHeight + leftRightHeight + frontBackHeight; // 720 + 800 + 720 = 2240 (increased from 1740)
    cv::Mat surroundView = cv::Mat::zeros(surroundHeight, surroundWidth, CV_8UC3);
    
    // Define regions for each camera with proper proportions:
    // +----------+----------+----------+
    // |    L1    |  FRONT   |    R1    |  
    // +----------+----------+----------+
    // |   LEFT   |   CAR    |  RIGHT   |
    // +----------+----------+----------+
    // |    L2    |   BACK   |    R2    |
    // +----------+----------+----------+
    
    int leftStart = 0;
    int centerStart = leftRightWidth;
    int rightStart = leftRightWidth + frontBackWidth;
    
    cv::Rect frontRegion(centerStart, 0, frontBackWidth, frontBackHeight);                    // Top center
    cv::Rect leftRegion(leftStart, frontBackHeight, leftRightWidth, leftRightHeight);        // Left center
    cv::Rect rightRegion(rightStart, frontBackHeight, leftRightWidth, leftRightHeight);      // Right center  
    cv::Rect backRegion(centerStart, frontBackHeight + leftRightHeight, frontBackWidth, frontBackHeight); // Bottom center
    cv::Rect carRegion(centerStart, frontBackHeight, frontBackWidth, leftRightHeight);       // Center (car area)
    
    // Copy processed images to their respective regions
    try {
        processedFront.copyTo(surroundView(frontRegion));
        processedLeft.copyTo(surroundView(leftRegion));
        processedRight.copyTo(surroundView(rightRegion));
        processedBack.copyTo(surroundView(backRegion));
        
        // Fill center region with a dark background for the car model
        cv::Scalar carAreaColor(40, 40, 40); // Dark gray for car area
        cv::rectangle(surroundView, carRegion, carAreaColor, -1);
        
        // Add a simple car indicator in the center
        cv::Point carCenter(centerStart + frontBackWidth/2, frontBackHeight + leftRightHeight/2);
        cv::Size carSize(frontBackWidth/4, leftRightHeight/6);
        cv::Rect carIndicator(carCenter.x - carSize.width/2, carCenter.y - carSize.height/2, 
                             carSize.width, carSize.height);
        cv::Scalar carColor(200, 200, 200); // Light gray car
        cv::rectangle(surroundView, carIndicator, carColor, -1);
        
        // Add direction arrow to show front of car
        cv::Point arrowStart(carCenter.x, carCenter.y - carSize.height/4);
        cv::Point arrowEnd(carCenter.x, carCenter.y - carSize.height/2 - 20);
        cv::arrowedLine(surroundView, arrowStart, arrowEnd, cv::Scalar(255, 255, 255), 3, 8, 0, 0.3);
        
        std::cout << "Surround view created - Size: " << surroundWidth << "x" << surroundHeight << std::endl;
        
        // Add borders between regions for visual separation
        cv::Scalar borderColor(100, 100, 100); // Gray border
        int borderThickness = 2;
        
        // Vertical borders - separating left, center, and right sections
        cv::line(surroundView, cv::Point(leftRightWidth, 0), cv::Point(leftRightWidth, surroundHeight), borderColor, borderThickness);
        cv::line(surroundView, cv::Point(leftRightWidth + frontBackWidth, 0), cv::Point(leftRightWidth + frontBackWidth, surroundHeight), borderColor, borderThickness);
        
        // Horizontal borders - separating top, middle, and bottom sections
        cv::line(surroundView, cv::Point(0, frontBackHeight), cv::Point(surroundWidth, frontBackHeight), borderColor, borderThickness);
        cv::line(surroundView, cv::Point(0, frontBackHeight + leftRightHeight), cv::Point(surroundWidth, frontBackHeight + leftRightHeight), borderColor, borderThickness);
        
        return surroundView;
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error creating surround view: " << e.what() << std::endl;
        return cv::Mat();
    }
}

// YAML Camera Parameter Loading Functions
bool ImageProcessor::loadCameraParameters(const std::string& cameraName) {
    // Use the single camera_intrinsics.yml file for all cameras
    std::vector<std::string> possiblePaths = {
        "camera_intrinsics.yml",
        "../camera_intrinsics.yml",
        "../../camera_intrinsics.yml"
    };
    
    cv::FileStorage fs;
    bool fileFound = false;
    std::string yamlPath;
    
    for (const auto& path : possiblePaths) {
        try {
            fs.open(path, cv::FileStorage::READ);
            if (fs.isOpened()) {
                yamlPath = path;
                fileFound = true;
                std::cout << "Loading camera parameters from: " << path << " for camera: " << cameraName << std::endl;
                break;
            }
        } catch (...) {
            continue;
        }
    }
    
    if (!fileFound) {
        std::cerr << "Could not open camera_intrinsics.yml file for camera: " << cameraName << std::endl;
        std::cerr << "Tried paths:" << std::endl;
        for (const auto& path : possiblePaths) {
            std::cerr << "  - " << path << std::endl;
        }
        return false;
    }
    
    try {
        cv::Mat cameraMatrix, distCoeffs, xiMat;
        
        // Read camera matrix (K)
        fs["K"] >> cameraMatrix;
        if (cameraMatrix.empty()) {
            std::cerr << "Failed to read K (camera matrix) from camera_intrinsics.yml" << std::endl;
            fs.release();
            return false;
        }
        
        // Read distortion coefficients (D)
        fs["D"] >> distCoeffs;
        if (distCoeffs.empty()) {
            std::cerr << "Failed to read D (distortion coefficients) from camera_intrinsics.yml" << std::endl;
            fs.release();
            return false;
        }
        
        // Read omnidirectional parameter (xi)
        fs["xi"] >> xiMat;
        double xi = 0.0;
        if (!xiMat.empty()) {
            xi = xiMat.at<double>(0, 0);
        }
        
        // Store parameters for this camera (all cameras use the same intrinsics)
        cameraMatrices[cameraName] = cameraMatrix.clone();
        distortionCoeffs[cameraName] = distCoeffs.clone();
        xiParams[cameraName] = xi;
        
        // Set default scale and shift values since they're not in the new format
        scaleXY[cameraName] = cv::Point2f(1.0f, 1.0f);
        shiftXY[cameraName] = cv::Point2f(0.0f, 0.0f);
        
        std::cout << "Successfully loaded parameters for camera: " << cameraName << std::endl;
        std::cout << "  Camera matrix (K) size: " << cameraMatrix.size() << std::endl;
        std::cout << "  Distortion coeffs (D) size: " << distCoeffs.size() << std::endl;
        std::cout << "  Xi parameter: " << xi << std::endl;
        std::cout << "  Scale XY: " << scaleXY[cameraName] << std::endl;
        std::cout << "  Shift XY: " << shiftXY[cameraName] << std::endl;
        
        fs.release();
        return true;
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error reading YAML parameters for " << cameraName << ": " << e.what() << std::endl;
        fs.release();
        return false;
    }
}

cv::Mat ImageProcessor::undistortWithYAMLParams(const cv::Mat& input, const std::string& cameraName) {
    if (input.empty()) return input;
    
    // Check if parameters are loaded for this camera
    if (cameraMatrices.find(cameraName) == cameraMatrices.end() || 
        distortionCoeffs.find(cameraName) == distortionCoeffs.end()) {
        std::cerr << "Camera parameters not loaded for: " << cameraName << std::endl;
        return input;
    }
    
    std::cout << "DEBUG: Undistorting with camera_intrinsics.yml params for camera: " << cameraName << std::endl;
    std::cout << "DEBUG: Input size: " << input.cols << "x" << input.rows << std::endl;
    
    cv::Mat cameraMatrix = cameraMatrices[cameraName];
    cv::Mat distCoeffs = distortionCoeffs[cameraName];
    double xi = xiParams[cameraName];
    
    std::cout << "DEBUG: Camera matrix (K): " << cameraMatrix << std::endl;
    std::cout << "DEBUG: Distortion coeffs (D): " << distCoeffs.t() << std::endl;
    std::cout << "DEBUG: Xi parameter: " << xi << std::endl;
    
    cv::Mat undistorted;
    try {
        // Create a more aggressive undistortion approach for fisheye cameras
        cv::Mat newCameraMatrix = cameraMatrix.clone();
        
        // For high xi values (omnidirectional/fisheye), be much more aggressive
        if (xi > 0.5) {
            std::cout << "DEBUG: High xi parameter detected (" << xi << "), applying aggressive fisheye undistortion" << std::endl;
            
            // Use front camera's successful undistortion parameters for all cameras
            // Significantly reduce focal length to zoom out and capture wider field after undistortion
            newCameraMatrix.at<double>(0, 0) *= 0.45; // fx (45% of original - same as front)
            newCameraMatrix.at<double>(1, 1) *= 0.45; // fy (45% of original - same as front)
            
            // Use fisheye undistortion for all cameras since it works well for front camera
            try {
                cv::fisheye::undistortImage(input, undistorted, cameraMatrix, distCoeffs, newCameraMatrix);
                std::cout << "DEBUG: Using fisheye undistortion model for camera: " << cameraName << std::endl;
            } catch (const cv::Exception& fe) {
                std::cout << "DEBUG: Fisheye undistortion failed, trying standard undistortion: " << fe.what() << std::endl;
                cv::undistort(input, undistorted, cameraMatrix, distCoeffs, newCameraMatrix);
            }
        } else {
            // Standard undistortion for lower xi values
            newCameraMatrix.at<double>(0, 0) *= 0.6; // fx (60% of original)
            newCameraMatrix.at<double>(1, 1) *= 0.6; // fy (60% of original)
            cv::undistort(input, undistorted, cameraMatrix, distCoeffs, newCameraMatrix);
        }
        
        // Remove the additional perspective correction for side cameras since 
        // we're now using the same undistortion approach as the front camera
        // if (cameraName == "left" || cameraName == "right") {
        //     undistorted = applyPerspectiveCorrection(undistorted, cameraName);
        // }
        
        // Enhanced cropping to remove car frame/body parts more effectively
        cv::Mat cropped = applyCameraSpecificCropping(undistorted, cameraName);
        
        // Apply scale and shift if available
        cv::Point2f scale = scaleXY[cameraName];
        cv::Point2f shift = shiftXY[cameraName];
        
        cv::Mat final = cropped;
        if (scale.x != 1.0f || scale.y != 1.0f || shift.x != 0.0f || shift.y != 0.0f) {
            std::cout << "DEBUG: Applying scale (" << scale.x << ", " << scale.y 
                      << ") and shift (" << shift.x << ", " << shift.y << ")" << std::endl;
            
            cv::Size newSize(static_cast<int>(cropped.cols * scale.x), 
                           static_cast<int>(cropped.rows * scale.y));
            cv::resize(cropped, final, newSize);
            
            // Apply shift (translation)
            cv::Mat translationMatrix = (cv::Mat_<double>(2, 3) << 1, 0, shift.x, 0, 1, shift.y);
            cv::warpAffine(final, final, translationMatrix, final.size());
        }
        
        // Check if the result is valid
        cv::Scalar meanVal = cv::mean(final);
        double totalMean = (meanVal[0] + meanVal[1] + meanVal[2]) / 3.0;
        
        if (totalMean < 5.0) {
            std::cerr << "WARNING: Undistorted image appears to be mostly black (mean=" << totalMean 
                      << ") for camera " << cameraName << ", using original" << std::endl;
            return input;
        }
        
        std::cout << "DEBUG: Aggressive fisheye undistortion successful for camera: " << cameraName << std::endl;
        return final;
        
    } catch (const cv::Exception& e) {
        std::cerr << "ERROR: camera_intrinsics.yml-based undistortion failed for " << cameraName << ": " << e.what() << std::endl;
        return input;
    }
}

// Image rotation functions
cv::Mat ImageProcessor::rotateImage90Clockwise(const cv::Mat& input) {
    cv::Mat output;
    cv::rotate(input, output, cv::ROTATE_90_CLOCKWISE);
    return output;
}

cv::Mat ImageProcessor::rotateImage90CounterClockwise(const cv::Mat& input) {
    cv::Mat output;
    cv::rotate(input, output, cv::ROTATE_90_COUNTERCLOCKWISE);
    return output;
}

cv::Mat ImageProcessor::rotateImage180(const cv::Mat& input) {
    cv::Mat output;
    cv::rotate(input, output, cv::ROTATE_180);
    return output;
}

// Extrinsic camera parameter functions
bool ImageProcessor::loadExtrinsicParameters() {
    std::string extrinsicsFile = "camera_extrinsics.csv";
    
    // Try different paths
    std::vector<std::string> paths = {
        extrinsicsFile,
        "../" + extrinsicsFile,
        "../../" + extrinsicsFile
    };
    
    std::ifstream file;
    std::string usedPath;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            usedPath = path;
            break;
        }
    }
    
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open camera_extrinsics.csv file" << std::endl;
        return false;
    }
    
    std::cout << "Loading extrinsic parameters from: " << usedPath << std::endl;
    
    std::string line;
    // Skip header line
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 7) {
            std::string cameraName = tokens[0];
            float pos_x = std::stof(tokens[1]);
            float pos_y = std::stof(tokens[2]);
            float pos_z = std::stof(tokens[3]);
            float rot_x = std::stof(tokens[4]); // pitch
            float rot_y = std::stof(tokens[5]); // yaw
            float rot_z = std::stof(tokens[6]); // roll
            
            // Store position and rotation
            cameraPositions[cameraName] = cv::Vec3f(pos_x, pos_y, pos_z);
            cameraRotations[cameraName] = cv::Vec3f(rot_x, rot_y, rot_z);
            
            // Create 4x4 extrinsic transformation matrix
            cv::Mat extrinsic = cv::Mat::eye(4, 4, CV_32F);
            
            // Convert rotations from degrees to radians
            float pitch = rot_x * CV_PI / 180.0f;
            float yaw = rot_y * CV_PI / 180.0f;
            float roll = rot_z * CV_PI / 180.0f;
            
            // Create rotation matrix (ZYX convention: Roll * Pitch * Yaw)
            cv::Mat R_x = (cv::Mat_<float>(3, 3) <<
                1, 0, 0,
                0, cos(pitch), -sin(pitch),
                0, sin(pitch), cos(pitch));
                
            cv::Mat R_y = (cv::Mat_<float>(3, 3) <<
                cos(yaw), 0, sin(yaw),
                0, 1, 0,
                -sin(yaw), 0, cos(yaw));
                
            cv::Mat R_z = (cv::Mat_<float>(3, 3) <<
                cos(roll), -sin(roll), 0,
                sin(roll), cos(roll), 0,
                0, 0, 1);
                
            cv::Mat R = R_z * R_y * R_x;
            
            // Set rotation part
            R.copyTo(extrinsic(cv::Rect(0, 0, 3, 3)));
            
            // Set translation part
            extrinsic.at<float>(0, 3) = pos_x;
            extrinsic.at<float>(1, 3) = pos_y;
            extrinsic.at<float>(2, 3) = pos_z;
            
            extrinsicMatrices[cameraName] = extrinsic;
            
            std::cout << "Loaded extrinsics for camera: " << cameraName 
                      << " Position: (" << pos_x << ", " << pos_y << ", " << pos_z << ")"
                      << " Rotation: (" << rot_x << ", " << rot_y << ", " << rot_z << ")" << std::endl;
        }
    }
    
    file.close();
    return true;
}

cv::Mat ImageProcessor::getExtrinsicMatrix(const std::string& cameraName) {
    auto it = extrinsicMatrices.find(cameraName);
    if (it != extrinsicMatrices.end()) {
        return it->second;
    }
    return cv::Mat::eye(4, 4, CV_32F); // Return identity if not found
}

cv::Vec3f ImageProcessor::getCameraPosition(const std::string& cameraName) {
    auto it = cameraPositions.find(cameraName);
    if (it != cameraPositions.end()) {
        return it->second;
    }
    return cv::Vec3f(0, 0, 0); // Return origin if not found
}

cv::Vec3f ImageProcessor::getCameraRotation(const std::string& cameraName) {
    auto it = cameraRotations.find(cameraName);
    if (it != cameraRotations.end()) {
        return it->second;
    }
    return cv::Vec3f(0, 0, 0); // Return no rotation if not found
}

cv::Mat ImageProcessor::applyCameraSpecificCropping(const cv::Mat& image, const std::string& cameraName) {
    if (image.empty()) return image;
    
    int width = image.cols;
    int height = image.rows;
    cv::Rect cropRect;
    
    std::cout << "DEBUG: Applying aggressive cropping for " << cameraName << " (size: " << width << "x" << height << ")" << std::endl;
    
    if (cameraName == "front") {
        // For front camera: remove bottom portion (car hood) and sides
        int cropTop = static_cast<int>(height * 0.15);    // Remove top 15%
        int cropBottom = static_cast<int>(height * 0.40); // Remove bottom 40% (car hood)
        int cropLeft = static_cast<int>(width * 0.15);    // Remove left 15%
        int cropRight = static_cast<int>(width * 0.15);   // Remove right 15%
        
        cropRect = cv::Rect(cropLeft, cropTop, 
                           width - cropLeft - cropRight, 
                           height - cropTop - cropBottom);
    }
    else if (cameraName == "back") {
        // For back camera: remove bottom portion (car trunk/rear) and sides
        int cropTop = static_cast<int>(height * 0.15);    // Remove top 15%
        int cropBottom = static_cast<int>(height * 0.35); // Remove bottom 35% (car rear)
        int cropLeft = static_cast<int>(width * 0.15);    // Remove left 15%
        int cropRight = static_cast<int>(width * 0.15);   // Remove right 15%
        
        cropRect = cv::Rect(cropLeft, cropTop, 
                           width - cropLeft - cropRight, 
                           height - cropTop - cropBottom);
    }
    else if (cameraName == "left") {
        // For left camera: shift view to the right to avoid car frame
        int cropTop = static_cast<int>(height * 0.15);    // Remove top 15%
        int cropBottom = static_cast<int>(height * 0.35); // Remove bottom 35% 
        // Shift the crop window to the right to look more rightward
        int cropLeft = static_cast<int>(width * 0.25);    // Start further right (was 0.10)
        int cropRight = static_cast<int>(width * 0.35);   // Reduce right crop (was 0.50)
        
        cropRect = cv::Rect(cropLeft, cropTop, 
                           width - cropLeft - cropRight, 
                           height - cropTop - cropBottom);
    }
    else if (cameraName == "right") {
        // For right camera: shift view to the left to avoid car frame
        int cropTop = static_cast<int>(height * 0.15);    // Remove top 15%
        int cropBottom = static_cast<int>(height * 0.35); // Remove bottom 35%
        // Shift the crop window to the left to look more leftward
        int cropLeft = static_cast<int>(width * 0.35);    // Reduce left crop (was 0.50) 
        int cropRight = static_cast<int>(width * 0.25);   // Start further left (was 0.10)
        
        cropRect = cv::Rect(cropLeft, cropTop, 
                           width - cropLeft - cropRight, 
                           height - cropTop - cropBottom);
    }
    else {
        // Default: minimal cropping for unknown cameras
        int cropAmount = static_cast<int>(std::min(width, height) * 0.10);
        cropRect = cv::Rect(cropAmount, cropAmount, 
                           width - 2 * cropAmount, 
                           height - 2 * cropAmount);
    }
    
    // Validate crop rectangle
    cropRect.x = std::max(0, cropRect.x);
    cropRect.y = std::max(0, cropRect.y);
    cropRect.width = std::min(cropRect.width, width - cropRect.x);
    cropRect.height = std::min(cropRect.height, height - cropRect.y);
    
    if (cropRect.width <= 0 || cropRect.height <= 0) {
        std::cerr << "WARNING: Invalid crop rectangle for " << cameraName << ", using original image" << std::endl;
        return image;
    }
    
    cv::Mat cropped = image(cropRect);
    std::cout << "DEBUG: Cropped " << cameraName << " from " << width << "x" << height 
              << " to " << cropped.cols << "x" << cropped.rows << std::endl;
    
    return cropped;
}

// Additional perspective correction for side cameras to reduce U-shaped fisheye warping
cv::Mat ImageProcessor::applyPerspectiveCorrection(const cv::Mat& input, const std::string& cameraName) {
    if (input.empty()) return input;
    
    std::cout << "DEBUG: Applying perspective correction for " << cameraName << " to reduce U-shaped warping" << std::endl;
    
    cv::Mat output;
    cv::Size size = input.size();
    
    // Define source and destination points for perspective transformation
    std::vector<cv::Point2f> srcPoints, dstPoints;
    
    if (cameraName == "left") {
        // For left camera: correct the rightward curve of the car frame
        srcPoints = {
            cv::Point2f(size.width * 0.1, size.height * 0.2),   // Top-left
            cv::Point2f(size.width * 0.9, size.height * 0.1),   // Top-right (pull inward)
            cv::Point2f(size.width * 0.1, size.height * 0.8),   // Bottom-left
            cv::Point2f(size.width * 0.9, size.height * 0.9)    // Bottom-right (pull inward)
        };
        
        dstPoints = {
            cv::Point2f(size.width * 0.1, size.height * 0.2),   // Top-left (unchanged)
            cv::Point2f(size.width * 0.8, size.height * 0.1),   // Top-right (straightened)
            cv::Point2f(size.width * 0.1, size.height * 0.8),   // Bottom-left (unchanged)
            cv::Point2f(size.width * 0.8, size.height * 0.9)    // Bottom-right (straightened)
        };
    }
    else if (cameraName == "right") {
        // For right camera: correct the leftward curve of the car frame
        srcPoints = {
            cv::Point2f(size.width * 0.1, size.height * 0.1),   // Top-left (pull inward)
            cv::Point2f(size.width * 0.9, size.height * 0.2),   // Top-right
            cv::Point2f(size.width * 0.1, size.height * 0.9),   // Bottom-left (pull inward)
            cv::Point2f(size.width * 0.9, size.height * 0.8)    // Bottom-right
        };
        
        dstPoints = {
            cv::Point2f(size.width * 0.2, size.height * 0.1),   // Top-left (straightened)
            cv::Point2f(size.width * 0.9, size.height * 0.2),   // Top-right (unchanged)
            cv::Point2f(size.width * 0.2, size.height * 0.9),   // Bottom-left (straightened)
            cv::Point2f(size.width * 0.9, size.height * 0.8)    // Bottom-right (unchanged)
        };
    }
    else {
        // No perspective correction needed for front/back cameras
        return input;
    }
    
    try {
        // Calculate perspective transformation matrix
        cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
        
        // Apply perspective transformation
        cv::warpPerspective(input, output, perspectiveMatrix, size, cv::INTER_LINEAR);
        
        std::cout << "DEBUG: Perspective correction applied successfully for " << cameraName << std::endl;
        return output;
        
    } catch (const cv::Exception& e) {
        std::cerr << "ERROR: Perspective correction failed for " << cameraName << ": " << e.what() << std::endl;
        return input;
    }
}

cv::Mat ImageProcessor::projectToBirdEye(const cv::Mat& input, const std::string& cameraName, float groundPlaneHeight) {
    if (input.empty()) return input;
    
    // Get camera intrinsic parameters
    auto itK = cameraMatrices.find(cameraName);
    if (itK == cameraMatrices.end()) {
        std::cerr << "ERROR: Camera matrix not found for " << cameraName << std::endl;
        return input;
    }
    
    // Get extrinsic parameters
    cv::Mat extrinsic = getExtrinsicMatrix(cameraName);
    cv::Mat K = itK->second;
    
    // Create bird's eye view transformation
    // This is a simplified projection - you may need to adjust based on your specific requirements
    cv::Mat output;
    
    // For now, return the undistorted input as placeholder
    // You can enhance this with proper homography calculation using extrinsics
    return input;
}

cv::Mat ImageProcessor::createSurroundViewWithExtrinsics(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    // For now, use the existing createSurroundView method
    // This can be enhanced to use extrinsic data for better alignment
    return createSurroundView(front, left, right, back);
}

// Multi-threading implementation
void ImageProcessor::initializeThreadPool(size_t numThreads) {
    if (m_threadPoolInitialized) {
        return;
    }
    
    m_numThreads = (numThreads == 0) ? std::thread::hardware_concurrency() : numThreads;
    m_threadPoolInitialized = true;
    
    std::cout << "Initialized thread pool with " << m_numThreads << " threads" << std::endl;
}

void ImageProcessor::shutdownThreadPool() {
    if (!m_threadPoolInitialized) {
        return;
    }
    
    m_threadPoolInitialized = false;
    std::cout << "Thread pool shutdown complete" << std::endl;
}

void ImageProcessor::processImageAsync(const cv::Mat& input, const std::string& cameraName, std::promise<cv::Mat>&& promise) {
    try {
        // Perform undistortion with YAML parameters
        cv::Mat undistorted = undistortWithYAMLParams(input, cameraName);
        
        // Apply camera-specific rotations
        cv::Mat processed;
        if (cameraName == "left") {
            processed = rotateImage90CounterClockwise(undistorted);
        } else if (cameraName == "right") {
            processed = rotateImage90Clockwise(undistorted);
        } else if (cameraName == "back") {
            processed = rotateImage180(undistorted);
        } else {
            processed = undistorted; // front camera, no rotation
        }
        
        promise.set_value(processed);
    } catch (const std::exception& e) {
        std::cerr << "Error processing " << cameraName << " camera: " << e.what() << std::endl;
        promise.set_exception(std::current_exception());
    }
}

cv::Mat ImageProcessor::createSurroundViewParallel(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    if (front.empty() || left.empty() || right.empty() || back.empty()) {
        std::cerr << "One or more camera images are empty!" << std::endl;
        return cv::Mat();
    }
    
    if (!m_threadPoolInitialized) {
        std::cout << "Thread pool not initialized, falling back to serial processing" << std::endl;
        return createSurroundView(front, left, right, back);
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create promises and futures for each camera
    std::promise<cv::Mat> frontPromise, leftPromise, rightPromise, backPromise;
    auto frontFuture = frontPromise.get_future();
    auto leftFuture = leftPromise.get_future();
    auto rightFuture = rightPromise.get_future();
    auto backFuture = backPromise.get_future();
    
    // Launch async tasks for each camera
    auto frontTask = std::async(std::launch::async, &ImageProcessor::processImageAsync, this, 
                               std::cref(front), "front", std::move(frontPromise));
    auto leftTask = std::async(std::launch::async, &ImageProcessor::processImageAsync, this, 
                              std::cref(left), "left", std::move(leftPromise));
    auto rightTask = std::async(std::launch::async, &ImageProcessor::processImageAsync, this, 
                               std::cref(right), "right", std::move(rightPromise));
    auto backTask = std::async(std::launch::async, &ImageProcessor::processImageAsync, this, 
                              std::cref(back), "back", std::move(backPromise));
    
    // Wait for all tasks to complete and get results
    cv::Mat processedFront, processedLeft, processedRight, processedBack;
    
    try {
        processedFront = frontFuture.get();
        processedLeft = leftFuture.get();
        processedRight = rightFuture.get();
        processedBack = backFuture.get();
    } catch (const std::exception& e) {
        std::cerr << "Error in parallel processing: " << e.what() << std::endl;
        return cv::Mat();
    }
    
    auto processing_time = std::chrono::high_resolution_clock::now();
    auto processing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(processing_time - start_time);
    
    // Use rectangular sections that preserve aspect ratio better and expand vertically
    int baseWidth = 720;   // Base width for each section
    int baseHeight = 640;  // Increased base height for each section (was 540)
    
    // Create different sized sections for better proportion:
    // Front/Back: wider sections (landscape orientation) with more vertical space
    // Left/Right: taller sections (portrait orientation after rotation)
    int frontBackWidth = baseWidth + 120;   // 840 pixels wide
    int frontBackHeight = baseHeight + 80;  // 720 pixels tall
    int leftRightWidth = baseWidth;         // 720 pixels wide  
    int leftRightHeight = baseHeight + 160; // 800 pixels tall
    
    // Resize images to their appropriate dimensions (can also be parallelized)
    std::vector<std::future<void>> resizeTasks;
    
    resizeTasks.push_back(std::async(std::launch::async, [&]() {
        cv::resize(processedFront, processedFront, cv::Size(frontBackWidth, frontBackHeight));
    }));
    
    resizeTasks.push_back(std::async(std::launch::async, [&]() {
        cv::resize(processedLeft, processedLeft, cv::Size(leftRightWidth, leftRightHeight));
    }));
    
    resizeTasks.push_back(std::async(std::launch::async, [&]() {
        cv::resize(processedRight, processedRight, cv::Size(leftRightWidth, leftRightHeight));
    }));
    
    resizeTasks.push_back(std::async(std::launch::async, [&]() {
        cv::resize(processedBack, processedBack, cv::Size(frontBackWidth, frontBackHeight));
    }));
    
    // Wait for all resize operations to complete
    for (auto& task : resizeTasks) {
        task.wait();
    }
    
    auto resize_time = std::chrono::high_resolution_clock::now();
    auto resize_duration = std::chrono::duration_cast<std::chrono::milliseconds>(resize_time - processing_time);
    
    // Calculate surround view dimensions based on the new layout
    int surroundWidth = frontBackWidth + leftRightWidth * 2;  // 840 + 720 + 720 = 2280
    int surroundHeight = frontBackHeight + leftRightHeight + frontBackHeight; // 720 + 800 + 720 = 2240
    cv::Mat surroundView = cv::Mat::zeros(surroundHeight, surroundWidth, CV_8UC3);
    
    // Define regions for each camera with proper proportions
    cv::Rect leftRegion(0, frontBackHeight, leftRightWidth, leftRightHeight);
    cv::Rect frontRegion(leftRightWidth, 0, frontBackWidth, frontBackHeight);
    cv::Rect rightRegion(leftRightWidth + frontBackWidth, frontBackHeight, leftRightWidth, leftRightHeight);
    cv::Rect backRegion(leftRightWidth, frontBackHeight + leftRightHeight, frontBackWidth, frontBackHeight);
    
    // Copy processed images to their respective regions
    processedLeft.copyTo(surroundView(leftRegion));
    processedFront.copyTo(surroundView(frontRegion));
    processedRight.copyTo(surroundView(rightRegion));
    processedBack.copyTo(surroundView(backRegion));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto composition_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - resize_time);
    
    std::cout << "Parallel processing times - Processing: " << processing_duration.count() 
              << "ms, Resize: " << resize_duration.count() 
              << "ms, Composition: " << composition_duration.count() 
              << "ms, Total: " << total_duration.count() << "ms" << std::endl;
    
    std::cout << "Parallel surround view created - Size: " << surroundView.cols << "x" << surroundView.rows << std::endl;
    return surroundView;
}
