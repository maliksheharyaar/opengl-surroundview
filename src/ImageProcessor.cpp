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

// Apply perspective warping for seamless surround view with proper stretching
cv::Mat ImageProcessor::applyPerspectiveWarpingForSurroundView(const cv::Mat& input, const std::string& cameraName, cv::Size outputSize) {
    if (input.empty()) return cv::Mat();
    
    // Define source points (corners of the input image)
    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(0, 0),                        // Top-left
        cv::Point2f(input.cols - 1, 0),           // Top-right
        cv::Point2f(input.cols - 1, input.rows - 1), // Bottom-right
        cv::Point2f(0, input.rows - 1)            // Bottom-left
    };
    
    // Define destination points based on camera position for bird's-eye warping
    std::vector<cv::Point2f> dstPoints;
    
    if (cameraName == "front") {
        // Front camera: stretch outward and upward for bird's-eye view
        dstPoints = {
            cv::Point2f(outputSize.width * 0.1f, 0),                           // Stretch top-left outward
            cv::Point2f(outputSize.width * 0.9f, 0),                           // Stretch top-right outward
            cv::Point2f(outputSize.width * 0.7f, outputSize.height * 0.8f),    // Pull bottom-right inward
            cv::Point2f(outputSize.width * 0.3f, outputSize.height * 0.8f)     // Pull bottom-left inward
        };
    }
    else if (cameraName == "back") {
        // Back camera: stretch outward and downward for bird's-eye view
        dstPoints = {
            cv::Point2f(outputSize.width * 0.3f, outputSize.height * 0.2f),    // Pull top-left inward
            cv::Point2f(outputSize.width * 0.7f, outputSize.height * 0.2f),    // Pull top-right inward
            cv::Point2f(outputSize.width * 0.9f, outputSize.height),           // Stretch bottom-right outward
            cv::Point2f(outputSize.width * 0.1f, outputSize.height)            // Stretch bottom-left outward
        };
    }
    else if (cameraName == "left") {
        // Left camera: stretch outward and to the left
        dstPoints = {
            cv::Point2f(0, outputSize.height * 0.1f),                          // Stretch top-left outward
            cv::Point2f(outputSize.width * 0.8f, outputSize.height * 0.3f),    // Pull top-right inward
            cv::Point2f(outputSize.width * 0.8f, outputSize.height * 0.7f),    // Pull bottom-right inward
            cv::Point2f(0, outputSize.height * 0.9f)                           // Stretch bottom-left outward
        };
    }
    else if (cameraName == "right") {
        // Right camera: stretch outward and to the right
        dstPoints = {
            cv::Point2f(outputSize.width * 0.2f, outputSize.height * 0.3f),    // Pull top-left inward
            cv::Point2f(outputSize.width, outputSize.height * 0.1f),           // Stretch top-right outward
            cv::Point2f(outputSize.width, outputSize.height * 0.9f),           // Stretch bottom-right outward
            cv::Point2f(outputSize.width * 0.2f, outputSize.height * 0.7f)     // Pull bottom-left inward
        };
    }
    else {
        // Default: no warping
        dstPoints = srcPoints;
    }
    
    // Calculate perspective transformation matrix
    cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    
    // Apply perspective transformation
    cv::Mat warpedImage;
    cv::warpPerspective(input, warpedImage, perspectiveMatrix, outputSize, cv::INTER_LINEAR);
    
    return warpedImage;
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

// Enhanced surround view with advanced warping and seamless stitching
cv::Mat ImageProcessor::createSurroundViewWithWarping(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    if (front.empty() || left.empty() || right.empty() || back.empty()) {
        std::cerr << "One or more camera images are empty!" << std::endl;
        return cv::Mat();
    }
    
    std::cout << "Creating enhanced surround view with advanced warping..." << std::endl;
    
    // Step 1: Apply ground plane projection to each camera view
    cv::Mat projectedFront = projectToGroundPlane(front, "front");
    cv::Mat projectedLeft = projectToGroundPlane(left, "left");
    cv::Mat projectedRight = projectToGroundPlane(right, "right");
    cv::Mat projectedBack = projectToGroundPlane(back, "back");
    
    // Step 2: Apply rotations as needed (left and right cameras)
    cv::Mat rotatedLeft = rotateImage90CounterClockwise(projectedLeft);
    cv::Mat rotatedRight = rotateImage90Clockwise(projectedRight);
    cv::Mat rotatedBack = rotateImage180(projectedBack);
    
    // Step 3: Create stitching masks for seamless blending
    cv::Mat frontMask = createStitchingMask(projectedFront, "front");
    cv::Mat leftMask = createStitchingMask(rotatedLeft, "left");
    cv::Mat rightMask = createStitchingMask(rotatedRight, "right");
    cv::Mat backMask = createStitchingMask(rotatedBack, "back");
    
    // Step 4: Standardize dimensions for seamless stitching
    cv::Size standardSize(800, 600); // Common size for all camera views
    cv::resize(projectedFront, projectedFront, standardSize);
    cv::resize(rotatedLeft, rotatedLeft, standardSize);
    cv::resize(rotatedRight, rotatedRight, standardSize);
    cv::resize(rotatedBack, rotatedBack, standardSize);
    cv::resize(frontMask, frontMask, standardSize);
    cv::resize(leftMask, leftMask, standardSize);
    cv::resize(rightMask, rightMask, standardSize);
    cv::resize(backMask, backMask, standardSize);
    
    // Step 5: Create the enhanced surround view layout
    int viewWidth = standardSize.width;
    int viewHeight = standardSize.height;
    int surroundWidth = viewWidth * 3;   // 3 columns: left, center, right
    int surroundHeight = viewHeight * 3; // 3 rows: top, middle, bottom
    
    cv::Mat enhancedSurroundView = cv::Mat::zeros(surroundHeight, surroundWidth, CV_8UC3);
    
    // Define regions for seamless layout
    cv::Rect frontRegion(viewWidth, 0, viewWidth, viewHeight);                          // Top center
    cv::Rect leftRegion(0, viewHeight, viewWidth, viewHeight);                         // Middle left
    cv::Rect rightRegion(viewWidth * 2, viewHeight, viewWidth, viewHeight);            // Middle right
    cv::Rect backRegion(viewWidth, viewHeight * 2, viewWidth, viewHeight);             // Bottom center
    cv::Rect carRegion(viewWidth, viewHeight, viewWidth, viewHeight);                  // Center (car area)
    
    // Step 6: Copy warped images to their regions
    try {
        projectedFront.copyTo(enhancedSurroundView(frontRegion));
        rotatedLeft.copyTo(enhancedSurroundView(leftRegion));
        rotatedRight.copyTo(enhancedSurroundView(rightRegion));
        rotatedBack.copyTo(enhancedSurroundView(backRegion));
        
        // Step 7: Create seamless transitions at corner regions using blending
        
        // Front-Left corner blending
        cv::Rect frontLeftCorner(0, 0, viewWidth, viewHeight);
        cv::Mat frontLeftBlend = blendImages(
            projectedFront, rotatedLeft, frontMask, leftMask
        );
        cv::resize(frontLeftBlend, frontLeftBlend, cv::Size(viewWidth, viewHeight));
        frontLeftBlend.copyTo(enhancedSurroundView(frontLeftCorner));
        
        // Front-Right corner blending
        cv::Rect frontRightCorner(viewWidth * 2, 0, viewWidth, viewHeight);
        cv::Mat frontRightBlend = blendImages(
            projectedFront, rotatedRight, frontMask, rightMask
        );
        cv::resize(frontRightBlend, frontRightBlend, cv::Size(viewWidth, viewHeight));
        frontRightBlend.copyTo(enhancedSurroundView(frontRightCorner));
        
        // Back-Left corner blending
        cv::Rect backLeftCorner(0, viewHeight * 2, viewWidth, viewHeight);
        cv::Mat backLeftBlend = blendImages(
            rotatedBack, rotatedLeft, backMask, leftMask
        );
        cv::resize(backLeftBlend, backLeftBlend, cv::Size(viewWidth, viewHeight));
        backLeftBlend.copyTo(enhancedSurroundView(backLeftCorner));
        
        // Back-Right corner blending
        cv::Rect backRightCorner(viewWidth * 2, viewHeight * 2, viewWidth, viewHeight);
        cv::Mat backRightBlend = blendImages(
            rotatedBack, rotatedRight, backMask, rightMask
        );
        cv::resize(backRightBlend, backRightBlend, cv::Size(viewWidth, viewHeight));
        backRightBlend.copyTo(enhancedSurroundView(backRightCorner));
        
        // Step 8: Add car representation in center
        cv::Scalar carAreaColor(40, 40, 40); // Dark background
        cv::rectangle(enhancedSurroundView, carRegion, carAreaColor, -1);
        
        // Add car outline
        cv::Point carCenter(carRegion.x + carRegion.width/2, carRegion.y + carRegion.height/2);
        cv::Size carSize(carRegion.width/4, carRegion.height/6);
        cv::Rect carIndicator(carCenter.x - carSize.width/2, carCenter.y - carSize.height/2, 
                             carSize.width, carSize.height);
        cv::Scalar carColor(180, 180, 180);
        cv::rectangle(enhancedSurroundView, carIndicator, carColor, -1);
        
        // Add direction indicator
        cv::Point arrowStart(carCenter.x, carCenter.y - carSize.height/4);
        cv::Point arrowEnd(carCenter.x, carCenter.y - carSize.height/2 - 15);
        cv::arrowedLine(enhancedSurroundView, arrowStart, arrowEnd, cv::Scalar(255, 255, 255), 2);
        
        std::cout << "Enhanced surround view created - Size: " << surroundWidth << "x" << surroundHeight << std::endl;
        std::cout << "Using advanced warping with ground plane projection and seamless stitching" << std::endl;
        
        return enhancedSurroundView;
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error creating enhanced surround view: " << e.what() << std::endl;
        return cv::Mat();
    }
}

cv::Mat ImageProcessor::createEnhancedSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    // This method provides a simpler interface for enhanced surround view
    return createSurroundViewWithWarping(front, left, right, back);
}

cv::Mat ImageProcessor::createStitchingMask(const cv::Mat& image, const std::string& cameraView) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    
    // Create feathered masks for each camera view to enable smooth blending
    int width = image.cols;
    int height = image.rows;
    int featherWidth = std::min(width, height) / 10; // 10% feather zone
    
    if (cameraView == "front") {
        // Front camera: full weight in center, fade at bottom
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float weight = 1.0f;
                
                // Fade at bottom edge
                if (y > height - featherWidth) {
                    weight = (float)(height - y) / featherWidth;
                }
                
                // Fade at left and right edges
                if (x < featherWidth) {
                    weight = std::min(weight, (float)x / featherWidth);
                }
                if (x > width - featherWidth) {
                    weight = std::min(weight, (float)(width - x) / featherWidth);
                }
                
                mask.at<uchar>(y, x) = (uchar)(weight * 255);
            }
        }
    }
    else if (cameraView == "back") {
        // Back camera: full weight in center, fade at top
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float weight = 1.0f;
                
                // Fade at top edge
                if (y < featherWidth) {
                    weight = (float)y / featherWidth;
                }
                
                // Fade at left and right edges
                if (x < featherWidth) {
                    weight = std::min(weight, (float)x / featherWidth);
                }
                if (x > width - featherWidth) {
                    weight = std::min(weight, (float)(width - x) / featherWidth);
                }
                
                mask.at<uchar>(y, x) = (uchar)(weight * 255);
            }
        }
    }
    else if (cameraView == "left") {
        // Left camera: full weight in center, fade at right
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float weight = 1.0f;
                
                // Fade at right edge
                if (x > width - featherWidth) {
                    weight = (float)(width - x) / featherWidth;
                }
                
                // Fade at top and bottom edges
                if (y < featherWidth) {
                    weight = std::min(weight, (float)y / featherWidth);
                }
                if (y > height - featherWidth) {
                    weight = std::min(weight, (float)(height - y) / featherWidth);
                }
                
                mask.at<uchar>(y, x) = (uchar)(weight * 255);
            }
        }
    }
    else if (cameraView == "right") {
        // Right camera: full weight in center, fade at left
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float weight = 1.0f;
                
                // Fade at left edge
                if (x < featherWidth) {
                    weight = (float)x / featherWidth;
                }
                
                // Fade at top and bottom edges
                if (y < featherWidth) {
                    weight = std::min(weight, (float)y / featherWidth);
                }
                if (y > height - featherWidth) {
                    weight = std::min(weight, (float)(height - y) / featherWidth);
                }
                
                mask.at<uchar>(y, x) = (uchar)(weight * 255);
            }
        }
    }
    else {
        // Default: uniform mask
        mask.setTo(255);
    }
    
    return mask;
}

cv::Mat ImageProcessor::blendImages(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask1, const cv::Mat& mask2) {
    if (img1.empty() || img2.empty() || mask1.empty() || mask2.empty()) {
        return cv::Mat();
    }
    
    // Ensure all images have the same size
    cv::Size blendSize = img1.size();
    cv::Mat resizedImg2, resizedMask1, resizedMask2;
    
    cv::resize(img2, resizedImg2, blendSize);
    cv::resize(mask1, resizedMask1, blendSize);
    cv::resize(mask2, resizedMask2, blendSize);
    
    // Convert masks to 3-channel for blending
    cv::Mat mask1_3ch, mask2_3ch;
    cv::cvtColor(resizedMask1, mask1_3ch, cv::COLOR_GRAY2BGR);
    cv::cvtColor(resizedMask2, mask2_3ch, cv::COLOR_GRAY2BGR);
    
    // Normalize masks
    mask1_3ch.convertTo(mask1_3ch, CV_32F, 1.0/255.0);
    mask2_3ch.convertTo(mask2_3ch, CV_32F, 1.0/255.0);
    
    // Convert images to float
    cv::Mat img1_f, img2_f;
    img1.convertTo(img1_f, CV_32F);
    resizedImg2.convertTo(img2_f, CV_32F);
    
    // Weighted blending
    cv::Mat totalWeight = mask1_3ch + mask2_3ch;
    cv::Mat blended = cv::Mat::zeros(blendSize, CV_32FC3);
    
    for (int y = 0; y < blendSize.height; y++) {
        for (int x = 0; x < blendSize.width; x++) {
            cv::Vec3f weight1 = mask1_3ch.at<cv::Vec3f>(y, x);
            cv::Vec3f weight2 = mask2_3ch.at<cv::Vec3f>(y, x);
            cv::Vec3f totalW = totalWeight.at<cv::Vec3f>(y, x);
            
            if (totalW[0] > 0 || totalW[1] > 0 || totalW[2] > 0) {
                cv::Vec3f pixel1 = img1_f.at<cv::Vec3f>(y, x);
                cv::Vec3f pixel2 = img2_f.at<cv::Vec3f>(y, x);
                
                cv::Vec3f blendedPixel = (pixel1.mul(weight1) + pixel2.mul(weight2));
                blendedPixel[0] /= std::max(totalW[0], 0.001f);
                blendedPixel[1] /= std::max(totalW[1], 0.001f);
                blendedPixel[2] /= std::max(totalW[2], 0.001f);
                
                blended.at<cv::Vec3f>(y, x) = blendedPixel;
            }
        }
    }
    
    // Convert back to 8-bit
    cv::Mat result;
    blended.convertTo(result, CV_8U);
    
    return result;
}

cv::Mat ImageProcessor::projectToGroundPlane(const cv::Mat& image, const std::string& cameraView, float groundHeight) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    // Use existing homography calculation but with ground plane projection
    cv::Mat homography = calculateGroundHomography(cameraView, image.size(), groundHeight);
    
    if (homography.empty()) {
        // If homography calculation fails, return the undistorted image
        std::cout << "Ground plane projection failed for " << cameraView << ", using undistorted image" << std::endl;
        return undistortWithYAMLParams(image, cameraView);
    }
    
    // Apply the ground plane transformation
    cv::Mat projectedImage;
    cv::warpPerspective(image, projectedImage, homography, image.size(), cv::INTER_LINEAR);
    
    return projectedImage;
}

cv::Mat ImageProcessor::calculateGroundHomography(const std::string& cameraView, const cv::Size& imageSize, float groundHeight) {
    // Create a simplified ground plane homography based on camera view
    // This is a simplified version - in practice, you'd use full camera calibration
    
    cv::Mat homography = cv::Mat::eye(3, 3, CV_32F);
    
    if (cameraView == "front") {
        // Front camera looking forward - slight perspective correction
        homography.at<float>(0, 0) = 1.0f;
        homography.at<float>(1, 1) = 0.8f;  // Compress vertically
        homography.at<float>(2, 1) = -0.001f; // Perspective effect
    }
    else if (cameraView == "back") {
        // Back camera looking backward - similar to front
        homography.at<float>(0, 0) = 1.0f;
        homography.at<float>(1, 1) = 0.8f;
        homography.at<float>(2, 1) = 0.001f; // Reverse perspective
    }
    else if (cameraView == "left") {
        // Left camera - side view projection
        homography.at<float>(0, 0) = 0.8f;   // Compress horizontally
        homography.at<float>(1, 1) = 1.0f;
        homography.at<float>(2, 0) = -0.001f; // Side perspective
    }
    else if (cameraView == "right") {
        // Right camera - side view projection
        homography.at<float>(0, 0) = 0.8f;
        homography.at<float>(1, 1) = 1.0f;
        homography.at<float>(2, 0) = 0.001f;  // Reverse side perspective
    }
    
    return homography;
}

// Truly seamless surround view without rigid grid structure
cv::Mat ImageProcessor::createSeamlessSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    if (front.empty() || left.empty() || right.empty() || back.empty()) {
        std::cerr << "One or more camera images are empty!" << std::endl;
        return cv::Mat();
    }
    
    std::cout << "Creating seamless surround view without grid constraints..." << std::endl;
    
    // Step 1: Create a larger canvas for seamless composition
    int canvasWidth = 2000;   // Much wider canvas for proper surround view
    int canvasHeight = 1600;  // Much taller canvas for better coverage
    cv::Mat seamlessView = cv::Mat::zeros(canvasHeight, canvasWidth, CV_8UC3);
    
    // Step 2: Apply advanced undistortion and prepare images
    cv::Mat processedFront = undistortWithYAMLParams(front, "front");
    cv::Mat processedLeft = undistortWithYAMLParams(left, "left");
    cv::Mat processedRight = undistortWithYAMLParams(right, "right");
    cv::Mat processedBack = undistortWithYAMLParams(back, "back");
    
    // Apply rotations - UPDATED to match cylindrical view
    cv::Mat rotatedLeft = rotateImage90CounterClockwise(processedLeft);  // Left: 90° counter-clockwise
    cv::Mat rotatedRight = rotateImage90Clockwise(processedRight);       // Right: 90° clockwise
    cv::Mat rotatedBack = rotateImage180(processedBack);                 // Back: 180° rotation
    cv::Mat rotatedFront = rotateImage180(processedFront);               // Front: 180° rotation
    
    // Step 3: Define extended regions for proper surround view
    int centerX = canvasWidth / 2;
    int centerY = canvasHeight / 2;
    int regionWidth = 700;   // Slightly smaller for better positioning
    int regionHeight = 550;  // Slightly smaller for better positioning
    int overlapSize = 250;   // Even larger overlap to eliminate gaps
    
    // Step 4: Create continuous blending masks with centered positioning
    cv::Mat frontMask = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32F);
    cv::Mat leftMask = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32F);
    cv::Mat rightMask = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32F);
    cv::Mat backMask = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32F);
    
    // Create radial masks centered around the car with equal spacing
    float radius = 350.0f;  // Distance from center for each camera view
    cv::Point2f frontCenter(centerX, centerY - radius);     // North
    cv::Point2f backCenter(centerX, centerY + radius);      // South  
    cv::Point2f leftCenter(centerX - radius, centerY);      // West
    cv::Point2f rightCenter(centerX + radius, centerY);     // East
    
    float maxRadius = 700.0f;  // Much larger influence radius to ensure overlap
    float minRadius = 80.0f;   // Smaller minimum radius for wider coverage
    
    // Generate smooth radial falloff masks with enhanced corner blending
    for (int y = 0; y < canvasHeight; y++) {
        for (int x = 0; x < canvasWidth; x++) {
            cv::Point2f current(x, y);
            
            // Calculate distances to each camera center
            float distToFront = cv::norm(current - frontCenter);
            float distToBack = cv::norm(current - backCenter);
            float distToLeft = cv::norm(current - leftCenter);
            float distToRight = cv::norm(current - rightCenter);
            
            // Calculate smooth falloff weights with extended coverage
            float frontWeight = std::max(0.0f, (maxRadius - distToFront) / (maxRadius - minRadius));
            float backWeight = std::max(0.0f, (maxRadius - distToBack) / (maxRadius - minRadius));
            float leftWeight = std::max(0.0f, (maxRadius - distToLeft) / (maxRadius - minRadius));
            float rightWeight = std::max(0.0f, (maxRadius - distToRight) / (maxRadius - minRadius));
            
            // Apply smooth cubic interpolation for better blending
            frontWeight = frontWeight * frontWeight * (3.0f - 2.0f * frontWeight);
            backWeight = backWeight * backWeight * (3.0f - 2.0f * backWeight);
            leftWeight = leftWeight * leftWeight * (3.0f - 2.0f * leftWeight);
            rightWeight = rightWeight * rightWeight * (3.0f - 2.0f * rightWeight);
            
            // Enhanced corner boost for complete gap elimination
            float centerX_f = static_cast<float>(centerX);
            float centerY_f = static_cast<float>(centerY);
            float distFromCenter = cv::norm(current - cv::Point2f(centerX_f, centerY_f));
            float cornerBoost = std::min(2.0f, 1.0f + (distFromCenter / 600.0f));  // Stronger corner boost
            
            // Apply smooth transitions with corner boost
            frontMask.at<float>(y, x) = std::min(1.0f, frontWeight * cornerBoost);
            backMask.at<float>(y, x) = std::min(1.0f, backWeight * cornerBoost);
            leftMask.at<float>(y, x) = std::min(1.0f, leftWeight * cornerBoost);
            rightMask.at<float>(y, x) = std::min(1.0f, rightWeight * cornerBoost);
        }
    }
    
    // Step 5: Apply perspective warping to create proper bird's-eye view stretching
    cv::Size warpSize(regionWidth + overlapSize, regionHeight + overlapSize);
    cv::Mat warpedFront = applyPerspectiveWarpingForSurroundView(rotatedFront, "front", warpSize);
    cv::Mat warpedLeft = applyPerspectiveWarpingForSurroundView(rotatedLeft, "left", warpSize);
    cv::Mat warpedRight = applyPerspectiveWarpingForSurroundView(rotatedRight, "right", warpSize);
    cv::Mat warpedBack = applyPerspectiveWarpingForSurroundView(rotatedBack, "back", warpSize);
    
    // Fallback to resizing if warping fails
    if (warpedFront.empty()) cv::resize(rotatedFront, warpedFront, warpSize);
    if (warpedLeft.empty()) cv::resize(rotatedLeft, warpedLeft, warpSize);
    if (warpedRight.empty()) cv::resize(rotatedRight, warpedRight, warpSize);
    if (warpedBack.empty()) cv::resize(rotatedBack, warpedBack, warpSize);
    
    // Step 6: Seamless composition using weighted blending with warped images
    cv::Mat frontContrib = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32FC3);
    cv::Mat leftContrib = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32FC3);
    cv::Mat rightContrib = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32FC3);
    cv::Mat backContrib = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32FC3);
    cv::Mat totalWeights = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32F);
    
    // Place and blend each camera's warped contribution with better centering
    placeImageWithMask(warpedFront, frontMask, frontContrib, totalWeights, 
                       frontCenter.x - (regionWidth + overlapSize)/2, frontCenter.y - (regionHeight + overlapSize)/2);
    placeImageWithMask(warpedLeft, leftMask, leftContrib, totalWeights,
                       leftCenter.x - (regionWidth + overlapSize)/2, leftCenter.y - (regionHeight + overlapSize)/2);
    placeImageWithMask(warpedRight, rightMask, rightContrib, totalWeights,
                       rightCenter.x - (regionWidth + overlapSize)/2, rightCenter.y - (regionHeight + overlapSize)/2);
    placeImageWithMask(warpedBack, backMask, backContrib, totalWeights,
                       backCenter.x - (regionWidth + overlapSize)/2, backCenter.y - (regionHeight + overlapSize)/2);
    
    // Step 7: Final blend with normalized weights and gap filling
    cv::Mat result = cv::Mat::zeros(canvasHeight, canvasWidth, CV_32FC3);
    result = frontContrib + leftContrib + rightContrib + backContrib;
    
    // Normalize by total weights and fill gaps with enhanced interpolation
    for (int y = 0; y < canvasHeight; y++) {
        for (int x = 0; x < canvasWidth; x++) {
            float totalWeight = totalWeights.at<float>(y, x);
            if (totalWeight > 0.001f) {
                cv::Vec3f pixel = result.at<cv::Vec3f>(y, x);
                result.at<cv::Vec3f>(y, x) = pixel / totalWeight;
            } else {
                // Enhanced gap filling with neighboring pixel interpolation
                cv::Vec3f fillColor(0.05f, 0.05f, 0.05f);  // Dark fill
                
                // Try to interpolate from nearby pixels
                int searchRadius = 15;
                cv::Vec3f avgColor(0, 0, 0);
                int validCount = 0;
                
                for (int dy = -searchRadius; dy <= searchRadius && validCount == 0; dy++) {
                    for (int dx = -searchRadius; dx <= searchRadius && validCount == 0; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if (ny >= 0 && ny < canvasHeight && nx >= 0 && nx < canvasWidth) {
                            if (totalWeights.at<float>(ny, nx) > 0.001f) {
                                avgColor = result.at<cv::Vec3f>(ny, nx);
                                validCount++;
                            }
                        }
                    }
                }
                
                result.at<cv::Vec3f>(y, x) = validCount > 0 ? avgColor : fillColor;
            }
        }
    }
    
    // Step 8: Add car representation in center without rigid boundaries
    cv::Point carCenter(centerX, centerY);
    int carWidth = 60, carHeight = 120;
    cv::Rect carRect(carCenter.x - carWidth/2, carCenter.y - carHeight/2, carWidth, carHeight);
    
    // Create car with soft edges
    cv::ellipse(result, carCenter, cv::Size(carWidth/2, carHeight/2), 0, 0, 360, 
                cv::Scalar(0.7, 0.7, 0.7), -1);
    
    // Add direction arrow
    cv::Point arrowStart(carCenter.x, carCenter.y - carHeight/3);
    cv::Point arrowEnd(carCenter.x, carCenter.y - carHeight/2 - 15);
    cv::arrowedLine(result, arrowStart, arrowEnd, cv::Scalar(1.0, 1.0, 1.0), 2);
    
    // Convert back to 8-bit
    cv::Mat finalResult;
    result.convertTo(finalResult, CV_8U, 255.0);
    
    std::cout << "Seamless surround view created - Size: " << canvasWidth << "x" << canvasHeight << std::endl;
    std::cout << "Using continuous radial blending with perspective warping for proper stretching" << std::endl;
    
    return finalResult;
}

// Computer vision-based cylindrical surround view with dynamic warping
cv::Mat ImageProcessor::createCylindricalSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back) {
    std::cout << "Creating cylindrical surround view with computer vision techniques..." << std::endl;
    
    // Validate input images
    if (front.empty() || left.empty() || right.empty() || back.empty()) {
        std::cout << "Error: One or more input images are empty" << std::endl;
        return cv::Mat();
    }
    
    try {
        // Step 1: Undistort and prepare images
        cv::Mat processedFront = undistortWithYAMLParams(front, "front");
        cv::Mat processedLeft = undistortWithYAMLParams(left, "left");
        cv::Mat processedRight = undistortWithYAMLParams(right, "right");
        cv::Mat processedBack = undistortWithYAMLParams(back, "back");
        
        // Validate undistorted images
        if (processedFront.empty() || processedLeft.empty() || processedRight.empty() || processedBack.empty()) {
            std::cout << "Warning: Undistortion failed, using original images" << std::endl;
            processedFront = front.clone();
            processedLeft = left.clone();
            processedRight = right.clone();
            processedBack = back.clone();
        }
        
        // Apply rotations to match real-world camera orientation - UPDATED WITH 180° FOR LEFT/RIGHT
        cv::Mat rotatedLeft = rotateImage180(processedLeft);                 // Left: 180° rotation
        cv::Mat rotatedRight = rotateImage180(processedRight);               // Right: 180° rotation  
        cv::Mat rotatedBack = rotateImage180(processedBack);                 // Back: 180° rotation
        cv::Mat rotatedFront = rotateImage180(processedFront);               // Front: 180° rotation
        
        // Step 2: Project all images to cylindrical coordinates
        std::cout << "Projecting images to cylindrical coordinates..." << std::endl;
        float focalLength = 650.0f; // Estimated from camera matrix
        
        std::cout << "Projecting front image..." << std::endl;
        cv::Mat cylFront = projectToCylindrical(rotatedFront, "front", focalLength);
        if (cylFront.empty()) {
            std::cout << "Error: Front cylindrical projection failed" << std::endl;
            return cv::Mat();
        }
        
        std::cout << "Projecting left image..." << std::endl;
        cv::Mat cylLeft = projectToCylindrical(rotatedLeft, "left", focalLength);
        if (cylLeft.empty()) {
            std::cout << "Error: Left cylindrical projection failed" << std::endl;
            return cv::Mat();
        }
        
        std::cout << "Projecting right image..." << std::endl;
        cv::Mat cylRight = projectToCylindrical(rotatedRight, "right", focalLength);
        if (cylRight.empty()) {
            std::cout << "Error: Right cylindrical projection failed" << std::endl;
            return cv::Mat();
        }
        
        std::cout << "Projecting back image..." << std::endl;
        cv::Mat cylBack = projectToCylindrical(rotatedBack, "back", focalLength);
        if (cylBack.empty()) {
            std::cout << "Error: Back cylindrical projection failed" << std::endl;
            return cv::Mat();
        }
        
        std::cout << "All cylindrical projections completed successfully" << std::endl;
        
        // Step 3: Create panoramic cylindrical canvas - INCREASED SCALE
        std::cout << "Creating cylindrical panoramic canvas..." << std::endl;
        int canvasWidth = 1600;   // Wide panoramic canvas for full 360-degree view
        int canvasHeight = 800;   // Height for bird's-eye view
        cv::Mat canvas = cv::Mat::zeros(canvasHeight, canvasWidth, CV_8UC3);
        
        // Step 4: Create seamless cylindrical panorama with proper warping - INCREASED SCALE
        std::cout << "Creating seamless cylindrical panorama with proper warping..." << std::endl;
        cv::Point2f canvasCenter(canvasWidth / 2.0f, canvasHeight / 2.0f);
        
        // Define camera placements with angular sectors
        struct CameraPlacement {
            cv::Mat image;
            float startAngle;    // Start angle in degrees 
            float endAngle;      // End angle in degrees
            float radius;        // Distance from center for cylindrical projection
            std::string name;
        };
        
        // Create overlapping angular sectors for seamless blending - FIXED: Correct left/right placement
        float sectorSize = 100.0f;  // Each camera covers 100 degrees with overlap
        std::vector<CameraPlacement> cameras = {
            {cylFront, 225.0f, 315.0f, 350.0f, "front"},  // Front: 225° to 315° (centered at 270°/top) - increased radius
            {cylLeft, 315.0f, 45.0f, 350.0f, "left"},     // Left: 315° to 45° (centered at 0°/right side) - FIXED: use cylLeft
            {cylBack, 45.0f, 135.0f, 350.0f, "back"},     // Back: 45° to 135° (centered at 90°/bottom) - increased radius
            {cylRight, 135.0f, 225.0f, 350.0f, "right"}   // Right: 135° to 225° (centered at 180°/left side) - FIXED: use cylRight
        };
        
        // Debug: Print camera sector information
        for (const auto& cam : cameras) {
            std::cout << "Camera " << cam.name << ": " << cam.startAngle << "° to " << cam.endAngle << "°" << std::endl;
        }
        
        // Create panoramic view by sampling from each camera based on angle
        for (int y = 0; y < canvasHeight; y++) {
            for (int x = 0; x < canvasWidth; x++) {
                // Calculate polar coordinates from canvas center
                float dx = x - canvasCenter.x;
                float dy = y - canvasCenter.y;
                float distance = std::sqrt(dx*dx + dy*dy);
                float angle = std::atan2(dy, dx) * 180.0f / CV_PI;
                if (angle < 0) angle += 360.0f;  // Normalize to 0-360
                
                // Only process pixels within the panoramic radius - INCREASED SCALE
                if (distance < 400.0f && distance > 80.0f) {
                    cv::Vec3f totalColor(0, 0, 0);
                    float totalWeight = 0.0f;
                    
                    // Check contribution from each camera
                    for (const auto& cam : cameras) {
                        if (cam.image.empty()) continue;
                        
                        // Check if this angle falls within camera's sector
                        bool inSector = false;
                        if (cam.startAngle <= cam.endAngle) {
                            // Normal case: sector doesn't wrap around 0°
                            inSector = (angle >= cam.startAngle && angle <= cam.endAngle);
                        } else {
                            // Wrap-around case (e.g., 315° to 45°)
                            inSector = (angle >= cam.startAngle || angle <= cam.endAngle);
                        }
                        
                        if (inSector) {
                            // Calculate sector center handling wrap-around
                            float sectorCenter;
                            if (cam.startAngle <= cam.endAngle) {
                                sectorCenter = (cam.startAngle + cam.endAngle) / 2.0f;
                            } else {
                                // Wrap-around case
                                float avgAngle = (cam.startAngle + cam.endAngle + 360.0f) / 2.0f;
                                sectorCenter = fmod(avgAngle, 360.0f);
                            }
                            
                            // Calculate angle offset from sector center
                            float angleOffset = angle - sectorCenter;
                            
                            // Handle wrap-around for angle offset calculation
                            if (angleOffset > 180.0f) angleOffset -= 360.0f;
                            if (angleOffset < -180.0f) angleOffset += 360.0f;
                            
                            // Map to image coordinates with camera-specific improved mapping
                            float normalizedOffset = angleOffset / (sectorSize / 2.0f);  // Normalize to [-1, 1]
                            
                            // Camera-specific horizontal mapping for better coverage
                            float imgX;
                            if (cam.name == "left" || cam.name == "right") {
                                // For side cameras, use more of the image width for better side coverage
                                imgX = cam.image.cols * (0.5f + 0.45f * normalizedOffset);
                            } else {
                                // For front/back cameras, standard mapping
                                imgX = cam.image.cols * (0.5f + 0.4f * normalizedOffset);
                            }
                            
                            float radialFactor = (distance - 80.0f) / 320.0f;  // Normalize distance - INCREASED SCALE
                            radialFactor = std::max(0.0f, std::min(1.0f, radialFactor));
                            
                            // Camera-specific vertical mapping for better perspective
                            float imgY;
                            if (cam.name == "left" || cam.name == "right") {
                                // For side cameras, use more vertical range for better side view
                                imgY = cam.image.rows * (0.15f + 0.7f * radialFactor);
                            } else {
                                // For front/back cameras, standard vertical mapping
                                imgY = cam.image.rows * (0.2f + 0.6f * radialFactor);
                            }
                            
                            // Bilinear interpolation
                            if (imgX >= 0 && imgX < cam.image.cols - 1 && imgY >= 0 && imgY < cam.image.rows - 1) {
                                int x0 = static_cast<int>(imgX);
                                int y0 = static_cast<int>(imgY);
                                int x1 = x0 + 1;
                                int y1 = y0 + 1;
                                
                                float fx = imgX - x0;
                                float fy = imgY - y0;
                                
                                cv::Vec3b p00 = cam.image.at<cv::Vec3b>(y0, x0);
                                cv::Vec3b p01 = cam.image.at<cv::Vec3b>(y0, x1);
                                cv::Vec3b p10 = cam.image.at<cv::Vec3b>(y1, x0);
                                cv::Vec3b p11 = cam.image.at<cv::Vec3b>(y1, x1);
                                
                                cv::Vec3f interpolated = 
                                    cv::Vec3f(p00) * (1-fx) * (1-fy) +
                                    cv::Vec3f(p01) * fx * (1-fy) +
                                    cv::Vec3f(p10) * (1-fx) * fy +
                                    cv::Vec3f(p11) * fx * fy;
                                
                                // Calculate blend weight based on distance from sector center
                                float blendWidth = 20.0f;  // Degrees of blending zone
                                float distToCenter = std::abs(angleOffset);
                                float weight = 1.0f;
                                
                                // Calculate sector half-width
                                float sectorHalfWidth = sectorSize / 2.0f;
                                
                                if (distToCenter > sectorHalfWidth - blendWidth) {
                                    float blendFactor = (sectorHalfWidth - distToCenter) / blendWidth;
                                    weight = std::max(0.0f, std::min(1.0f, blendFactor));
                                    weight = weight * weight * (3.0f - 2.0f * weight); // Smooth step function
                                }
                                
                                // Additional radial falloff for smooth edge blending - INCREASED SCALE
                                float radialWeight = 1.0f;
                                if (distance > 380.0f) {
                                    radialWeight = (400.0f - distance) / 20.0f;
                                    radialWeight = std::max(0.0f, std::min(1.0f, radialWeight));
                                } else if (distance < 100.0f) {
                                    radialWeight = (distance - 80.0f) / 20.0f;
                                    radialWeight = std::max(0.0f, std::min(1.0f, radialWeight));
                                }
                                
                                weight *= radialWeight;
                                
                                if (weight > 0.01f) {
                                    totalColor += interpolated * weight;
                                    totalWeight += weight;
                                }
                            }
                        }
                    }
                    
                    // Set final pixel value
                    if (totalWeight > 0.01f) {
                        cv::Vec3b finalColor;
                        cv::Vec3f normalizedColor = totalColor / totalWeight;
                        finalColor[0] = cv::saturate_cast<uchar>(normalizedColor[0]);
                        finalColor[1] = cv::saturate_cast<uchar>(normalizedColor[1]);
                        finalColor[2] = cv::saturate_cast<uchar>(normalizedColor[2]);
                        canvas.at<cv::Vec3b>(y, x) = finalColor;
                    }
                }
            }
        }
        // Add car representation at center
        cv::Point carCenter(canvas.cols/2, canvas.rows/2);
        int carWidth = 80, carHeight = 120;
        cv::Rect carRect(carCenter.x - carWidth/2, carCenter.y - carHeight/2, carWidth, carHeight);
        
        if (carRect.x >= 0 && carRect.y >= 0 && 
            carRect.x + carRect.width < canvas.cols && 
            carRect.y + carRect.height < canvas.rows) {
            cv::rectangle(canvas, carRect, cv::Scalar(255, 255, 255), -1);
            cv::rectangle(canvas, carRect, cv::Scalar(0, 0, 0), 3);
        }
        
        std::cout << "Seamless cylindrical surround view created - Size: " << canvas.cols << "x" << canvas.rows << std::endl;
        std::cout << "Using advanced warping and angular blending for seamless transitions" << std::endl;
        
        return canvas;
        
    } catch (const std::exception& e) {
        std::cout << "Exception in cylindrical surround view creation: " << e.what() << std::endl;
        return cv::Mat();
    } catch (...) {
        std::cout << "Unknown exception in cylindrical surround view creation" << std::endl;
        return cv::Mat();
    }
}

// Project image to cylindrical coordinates for panoramic stitching
cv::Mat ImageProcessor::projectToCylindrical(const cv::Mat& input, const std::string& cameraName, float focalLength) {
    if (input.empty()) {
        std::cout << "Error: Empty input to projectToCylindrical for camera: " << cameraName << std::endl;
        return cv::Mat();
    }
    
    std::cout << "Projecting " << cameraName << " camera image (size: " << input.cols << "x" << input.rows << ") to cylindrical..." << std::endl;
    
    cv::Mat output = cv::Mat::zeros(input.size(), input.type());
    cv::Point2f center(input.cols / 2.0f, input.rows / 2.0f);
    
    int processedPixels = 0;
    for (int y = 0; y < output.rows; y++) {
        for (int x = 0; x < output.cols; x++) {
            // Convert to cylindrical coordinates
            cv::Point2f cylPoint = cartesianToCylindrical(cv::Point2f(x, y), focalLength, center);
            
            // Safety bounds checking
            int srcX = static_cast<int>(std::round(cylPoint.x));
            int srcY = static_cast<int>(std::round(cylPoint.y));
            
            // Map back to input image coordinates with bounds checking
            if (srcX >= 0 && srcX < input.cols && srcY >= 0 && srcY < input.rows) {
                output.at<cv::Vec3b>(y, x) = input.at<cv::Vec3b>(srcY, srcX);
                processedPixels++;
            }
        }
    }
    
    std::cout << "Cylindrical projection for " << cameraName << " completed. Processed " << processedPixels << " pixels." << std::endl;
    return output;
}

// Convert cartesian to cylindrical coordinates
cv::Point2f ImageProcessor::cartesianToCylindrical(const cv::Point2f& point, float focalLength, const cv::Point2f& center) {
    float x = point.x - center.x;
    float y = point.y - center.y;
    
    // Safety check to avoid extreme values
    if (std::abs(x) < 1e-6) x = 1e-6f;
    
    // Cylindrical projection with bounds checking
    float theta = std::atan2(x, focalLength);
    float h = y / std::sqrt(x*x + focalLength*focalLength) * focalLength;
    
    // Map to image coordinates with clamping
    float cylX = center.x + focalLength * theta;
    float cylY = center.y + h;
    
    // Clamp to image bounds
    cylX = std::max(0.0f, std::min(static_cast<float>(center.x * 2), cylX));
    cylY = std::max(0.0f, std::min(static_cast<float>(center.y * 2), cylY));
    
    return cv::Point2f(cylX, cylY);
}

// Convert cylindrical to cartesian coordinates
cv::Point2f ImageProcessor::cylindricalToCartesian(const cv::Point2f& cylPoint, float focalLength, const cv::Point2f& center) {
    float theta = (cylPoint.x - center.x) / focalLength;
    float h = cylPoint.y - center.y;
    
    // Inverse cylindrical projection
    float x = focalLength * std::tan(theta);
    float y = h * std::sqrt(x*x + focalLength*focalLength) / focalLength;
    
    return cv::Point2f(center.x + x, center.y + y);
}

// Create blending mask with soft falloff
cv::Mat ImageProcessor::createBlendingMask(const cv::Mat& image, const cv::Point2f& center, float radius, float featherWidth) {
    cv::Mat mask = cv::Mat::zeros(image.rows, image.cols, CV_32F);
    
    for (int y = 0; y < mask.rows; y++) {
        for (int x = 0; x < mask.cols; x++) {
            float dist = cv::norm(cv::Point2f(x, y) - center);
            
            if (dist <= radius - featherWidth) {
                mask.at<float>(y, x) = 1.0f;
            } else if (dist <= radius) {
                // Smooth falloff in feather region
                float t = (radius - dist) / featherWidth;
                mask.at<float>(y, x) = t * t * (3.0f - 2.0f * t); // Smooth step function
            }
        }
    }
    
    return mask;
}

// Correct perspective distortion for bird's-eye view
cv::Mat ImageProcessor::correctPerspectiveDistortion(const cv::Mat& input, const std::string& cameraName, float vehicleHeight) {
    if (input.empty()) return cv::Mat();
    
    // Define perspective correction based on vehicle-mounted camera geometry
    cv::Point2f srcPoints[4] = {
        cv::Point2f(input.cols * 0.2f, input.rows * 0.3f),  // Top-left
        cv::Point2f(input.cols * 0.8f, input.rows * 0.3f),  // Top-right
        cv::Point2f(input.cols * 0.9f, input.rows * 0.9f),  // Bottom-right
        cv::Point2f(input.cols * 0.1f, input.rows * 0.9f)   // Bottom-left
    };
    
    cv::Point2f dstPoints[4] = {
        cv::Point2f(input.cols * 0.1f, input.rows * 0.1f),  // Top-left
        cv::Point2f(input.cols * 0.9f, input.rows * 0.1f),  // Top-right
        cv::Point2f(input.cols * 0.9f, input.rows * 0.9f),  // Bottom-right
        cv::Point2f(input.cols * 0.1f, input.rows * 0.9f)   // Bottom-left
    };
    
    cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    cv::Mat corrected;
    cv::warpPerspective(input, corrected, perspectiveMatrix, input.size());
    
    return corrected;
}

// Helper function to place image with mask blending
void ImageProcessor::placeImageWithMask(const cv::Mat& image, const cv::Mat& mask, cv::Mat& contribution, 
                                       cv::Mat& totalWeights, int startX, int startY) {
    // Convert image to float for blending
    cv::Mat imageFloat;
    image.convertTo(imageFloat, CV_32FC3, 1.0/255.0);
    
    int imgHeight = image.rows;
    int imgWidth = image.cols;
    int canvasHeight = contribution.rows;
    int canvasWidth = contribution.cols;
    
    for (int y = 0; y < imgHeight; y++) {
        for (int x = 0; x < imgWidth; x++) {
            int canvasX = startX + x;
            int canvasY = startY + y;
            
            // Check bounds
            if (canvasX >= 0 && canvasX < canvasWidth && canvasY >= 0 && canvasY < canvasHeight) {
                float weight = mask.at<float>(canvasY, canvasX);
                if (weight > 0.0f) {
                    cv::Vec3f pixel = imageFloat.at<cv::Vec3f>(y, x);
                    contribution.at<cv::Vec3f>(canvasY, canvasX) += pixel * weight;
                    totalWeights.at<float>(canvasY, canvasX) += weight;
                }
            }
        }
    }
}
