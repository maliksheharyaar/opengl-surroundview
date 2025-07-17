#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position), m_worldUp(up), m_yaw(yaw), m_pitch(pitch),
      m_movementSpeed(15.0f), m_mouseSensitivity(0.2f), m_fov(120.0f) {  // Increased FOV for wider view
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 100.0f);
}

void Camera::processKeyboard(int direction, float deltaTime) {
    // Keyboard movement disabled - camera position is fixed
    // Only zoom via scroll wheel is allowed
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    // Mouse movement disabled - camera orientation is fixed looking down
}

void Camera::processMouseScroll(float yoffset) {
    // Allow zoom in/out by adjusting FOV
    m_fov -= yoffset * 2.0f; // Increase sensitivity for better zoom control
    if (m_fov < 10.0f) m_fov = 10.0f;   // Max zoom in
    if (m_fov > 150.0f) m_fov = 150.0f;   // Max zoom out - increased for wider view
}

void Camera::maintainTopDownView() {
    // Ensure camera always looks straight down, rotated 180° clockwise
    m_yaw = 90.0f;  // Changed from -90.0f to +90.0f for 180° rotation
    m_pitch = -90.0f;
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    // Calculate new front vector
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    
    // Calculate right and up vectors
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
