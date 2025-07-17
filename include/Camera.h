#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Camera movement directions
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float FOV = 45.0f;

class Camera {
public:
    // Camera Attributes
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;
    
    // Euler Angles
    float m_yaw;
    float m_pitch;
    
    // Camera options
    float m_movementSpeed;
    float m_mouseSensitivity;
    float m_fov;

    // Constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), 
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), 
           float yaw = YAW, 
           float pitch = PITCH);

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 getViewMatrix() const;
    
    // Returns the projection matrix
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    
    // Returns camera position
    glm::vec3 getPosition() const { return m_position; }

    // Processes input received from any keyboard-like input system
    void processKeyboard(int direction, float deltaTime);

    // Processes input received from a mouse input system
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    // Processes input received from a mouse scroll-wheel event
    void processMouseScroll(float yoffset);
    
    // Maintains fixed top-down view
    void maintainTopDownView();

private:
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors();
};
