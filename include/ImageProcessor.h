/**
 * @file ImageProcessor.h
 * @brief Advanced image processing pipeline for multi-camera surround view systems
 * 
 * This class provides comprehensive image processing capabilities including:
 * - Fisheye lens undistortion with camera intrinsic parameters
 * - Multi-threaded parallel processing for real-time performance
 * - Cylindrical projection for seamless 360° surround view
 * - Camera calibration using YAML configuration files
 * - Advanced blending and stitching algorithms
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <map>
#include <thread>
#include <future>
#include <vector>
#include <mutex>

/**
 * @class ImageProcessor
 * @brief Main image processing engine for surround view generation
 */
class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    // Core image processing pipeline
    cv::Mat loadImage(const std::string& filepath);
    cv::Mat processFullPipeline(const cv::Mat& input);
    cv::Mat createSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Multi-threaded processing for performance optimization
    cv::Mat createSurroundViewParallel(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    void processImageAsync(const cv::Mat& input, const std::string& cameraName, std::promise<cv::Mat>&& promise);
    
    // Thread pool management
    void initializeThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    void shutdownThreadPool();
    
    // Development and testing utilities
    cv::Mat generateTestGrid(int width = 800, int height = 600);
    cv::Mat generateCheckerboard(int width, int height, int squareSize = 50);

    // Basic image operations
    void saveImage(const cv::Mat& image, const std::string& filepath);
    cv::Mat preprocessImage(const cv::Mat& input);
    
    // Perspective transformation and bird's-eye view
    cv::Mat createBirdEyeView(const cv::Mat& input);
    cv::Mat applyPerspectiveTransform(const cv::Mat& input, const cv::Mat& transformMatrix);
    cv::Mat getDefaultPerspectiveMatrix(int width, int height);
    
    // Fisheye lens correction
    cv::Mat undistortFisheye(const cv::Mat& input);
    cv::Mat applyHomography(const cv::Mat& input);
    cv::Mat getFrontCameraHomography(int width, int height);
    
    // Image rotation utilities
    cv::Mat rotateImage90Clockwise(const cv::Mat& input);
    cv::Mat rotateImage90CounterClockwise(const cv::Mat& input);
    cv::Mat rotateImage180(const cv::Mat& input);
    // Camera calibration and distortion correction
    bool loadCameraParameters(const std::string& cameraName);
    cv::Mat undistortWithYAMLParams(const cv::Mat& input, const std::string& cameraName);
    
    // Extrinsic camera parameter handling
    bool loadExtrinsicParameters();
    cv::Mat getExtrinsicMatrix(const std::string& cameraName);
    cv::Vec3f getCameraPosition(const std::string& cameraName);
    cv::Vec3f getCameraRotation(const std::string& cameraName);
    cv::Mat createSurroundViewWithExtrinsics(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Advanced surround view algorithms
    cv::Mat createSurroundViewWithWarping(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    cv::Mat createEnhancedSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Computer vision-based stitching and warping
    cv::Mat calculateHomographyMatrix(const std::string& cameraName, const cv::Size& imageSize);
    cv::Mat applyBirdEyeProjection(const cv::Mat& input, const std::string& cameraName);
    cv::Mat createStitchingMask(const cv::Mat& image, const std::string& cameraName);
    
    // Perspective warping for seamless integration
    cv::Mat applyPerspectiveWarpingForSurroundView(const cv::Mat& input, const std::string& cameraName, cv::Size outputSize);
    
    // Feature detection and matching for stitching
    std::vector<cv::Point2f> findStitchingPoints(const cv::Mat& img1, const cv::Mat& img2);
    cv::Mat warpToCommonPlane(const cv::Mat& input, const std::string& fromCamera, const std::string& toCamera);
    
    // Ground plane projection algorithms
    cv::Mat projectToGroundPlane(const cv::Mat& input, const std::string& cameraName, float groundHeight = 0.0f);
    
    // Advanced blending techniques
    cv::Mat blendImages(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask1, const cv::Mat& mask2);
    
    // Homography calculation for bird's-eye view transformation
    cv::Mat calculateGroundHomography(const std::string& cameraView, const cv::Size& imageSize, float groundHeight = 0.0f);

    // Camera-specific image enhancement
    cv::Mat applyCameraSpecificCropping(const cv::Mat& image, const std::string& cameraName);
    cv::Mat applyPerspectiveCorrection(const cv::Mat& input, const std::string& cameraName);
    cv::Mat projectToBirdEye(const cv::Mat& input, const std::string& cameraName, float groundPlaneHeight = 0.0f);

    // Seamless surround view generation
    cv::Mat createSeamlessSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Advanced blending helper functions
    void placeImageWithMask(const cv::Mat& image, const cv::Mat& mask, cv::Mat& contribution, cv::Mat& totalWeights, int startX, int startY);

    // Cylindrical projection for 360° surround view
    cv::Mat createCylindricalSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Cylindrical projection for seamless panoramic stitching
    cv::Mat projectToCylindrical(const cv::Mat& input, const std::string& cameraName, float focalLength = 800.0f);
    cv::Mat projectToSpherical(const cv::Mat& input, const std::string& cameraName, float focalLength = 800.0f);
    
    // Feature-based image alignment and warping
    cv::Mat alignAndWarpImage(const cv::Mat& source, const cv::Mat& target, const std::string& cameraName);
    std::vector<cv::KeyPoint> detectFeatures(const cv::Mat& image);
    std::vector<cv::DMatch> matchFeatures(const cv::Mat& desc1, const cv::Mat& desc2);
    cv::Mat calculateWarpMatrix(const std::vector<cv::Point2f>& srcPoints, const std::vector<cv::Point2f>& dstPoints);
    
    // Gradient-domain blending for seamless transitions
    cv::Mat gradientDomainBlending(const std::vector<cv::Mat>& images, const std::vector<cv::Mat>& masks, const cv::Size& outputSize);
    cv::Mat createBlendingMask(const cv::Mat& image, const cv::Point2f& center, float radius, float featherWidth = 50.0f);
    
    // Dynamic mesh warping for flexible image deformation
    cv::Mat applyMeshWarping(const cv::Mat& input, const std::vector<cv::Point2f>& srcPoints, const std::vector<cv::Point2f>& dstPoints);
    std::vector<cv::Point2f> generateMeshGrid(const cv::Size& imageSize, int gridSize = 20);
    
    // Cylindrical coordinate transformation
    cv::Point2f cartesianToCylindrical(const cv::Point2f& point, float focalLength, const cv::Point2f& center);
    cv::Point2f cylindricalToCartesian(const cv::Point2f& cylPoint, float focalLength, const cv::Point2f& center);
    
    // Advanced perspective correction for ground plane projection
    cv::Mat correctPerspectiveDistortion(const cv::Mat& input, const std::string& cameraName, float vehicleHeight = 1.5f);
    cv::Mat createGroundPlaneMapping(const std::string& cameraName, const cv::Size& imageSize, float groundHeight = 0.0f);

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
