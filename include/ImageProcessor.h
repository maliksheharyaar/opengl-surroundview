#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <map>
#include <thread>
#include <future>
#include <vector>
#include <mutex>

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    // Load and process images
    cv::Mat loadImage(const std::string& filepath);
    cv::Mat processFullPipeline(const cv::Mat& input); // Undistort + Homography
    cv::Mat createSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Multi-threaded processing
    cv::Mat createSurroundViewParallel(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    void processImageAsync(const cv::Mat& input, const std::string& cameraName, std::promise<cv::Mat>&& promise);
    
    // Thread pool management
    void initializeThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    void shutdownThreadPool();
    
    // Generate test patterns for development
    cv::Mat generateTestGrid(int width = 800, int height = 600);
    cv::Mat generateCheckerboard(int width, int height, int squareSize = 50);

    // Utility functions
    void saveImage(const cv::Mat& image, const std::string& filepath);
    cv::Mat preprocessImage(const cv::Mat& input);
    
    // Perspective transformation
    cv::Mat createBirdEyeView(const cv::Mat& input);
    cv::Mat applyPerspectiveTransform(const cv::Mat& input, const cv::Mat& transformMatrix);
    cv::Mat getDefaultPerspectiveMatrix(int width, int height);
    
    // Fisheye undistortion
    cv::Mat undistortFisheye(const cv::Mat& input);
    cv::Mat applyHomography(const cv::Mat& input);
    cv::Mat getFrontCameraHomography(int width, int height);
    
    // Image rotation functions
    cv::Mat rotateImage90Clockwise(const cv::Mat& input);
    cv::Mat rotateImage90CounterClockwise(const cv::Mat& input);
    cv::Mat rotateImage180(const cv::Mat& input);

    // Camera calibration from YAML files
    bool loadCameraParameters(const std::string& cameraName);
    cv::Mat undistortWithYAMLParams(const cv::Mat& input, const std::string& cameraName);
    
    // Extrinsic camera parameters
    bool loadExtrinsicParameters();
    cv::Mat getExtrinsicMatrix(const std::string& cameraName);
    cv::Vec3f getCameraPosition(const std::string& cameraName);
    cv::Vec3f getCameraRotation(const std::string& cameraName);
    cv::Mat createSurroundViewWithExtrinsics(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Camera-specific cropping to remove car frame/body parts
    cv::Mat applyCameraSpecificCropping(const cv::Mat& image, const std::string& cameraName);
    cv::Mat applyPerspectiveCorrection(const cv::Mat& input, const std::string& cameraName);
    cv::Mat projectToBirdEye(const cv::Mat& input, const std::string& cameraName, float groundPlaneHeight = 0.0f);

private:
    // Fisheye camera parameters
    void getFisheyeCameraParams(cv::Mat& cameraMatrix, cv::Mat& distCoeffs, cv::Size imageSize);
    
    // YAML camera parameters storage
    std::map<std::string, cv::Mat> cameraMatrices;
    std::map<std::string, cv::Mat> distortionCoeffs;
    std::map<std::string, cv::Mat> projectMatrices;
    std::map<std::string, cv::Point2f> scaleXY;
    std::map<std::string, cv::Point2f> shiftXY;
    std::map<std::string, double> xiParams; // Omnidirectional parameter
    
    // Extrinsic camera parameters storage
    std::map<std::string, cv::Vec3f> cameraPositions;  // Position relative to rear axle center
    std::map<std::string, cv::Vec3f> cameraRotations;  // Rotation in degrees (pitch, yaw, roll)
    std::map<std::string, cv::Mat> extrinsicMatrices;  // 4x4 transformation matrices
    
    // Multi-threading support
    bool m_threadPoolInitialized;
    size_t m_numThreads;
    std::mutex m_processingMutex;  // Protect shared resources during parallel processing
};
