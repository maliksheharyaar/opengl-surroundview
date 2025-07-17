#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <map>

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    // Load and process images
    cv::Mat loadImage(const std::string& filepath);
    cv::Mat processFullPipeline(const cv::Mat& input); // Undistort + Homography
    cv::Mat createSurroundView(const cv::Mat& front, const cv::Mat& left, const cv::Mat& right, const cv::Mat& back);
    
    // Generate test patterns for development
    cv::Mat generateTestGrid(int width = 800, int height = 600);

    // Utility functions
    void saveImage(const cv::Mat& image, const std::string& filepath);
    
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
    
    // Camera-specific cropping to remove car frame/body parts
    cv::Mat applyCameraSpecificCropping(const cv::Mat& image, const std::string& cameraName);

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
};
