#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include "Camera.h"

class Shader;
class Mesh;
#ifdef HAVE_ASSIMP
class Model;
#endif

class Renderer3D {
public:
    Renderer3D(int width, int height);
    ~Renderer3D();

    bool initialize();
    void render();
    void updateTexture(const cv::Mat& image);
    void cleanup();
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();

    // Camera controls
    void updateCamera(float deltaTime);
    void setMouseCallback();

private:
    // Window management
    GLFWwindow* m_window;
    int m_width, m_height;

    // Rendering components
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Mesh> m_planeMesh;
    std::unique_ptr<Mesh> m_carMesh;
#ifdef HAVE_ASSIMP
    std::unique_ptr<Model> m_carModel;
#endif

    // OpenGL objects
    GLuint m_textureID;
    GLuint m_VAO, m_VBO, m_EBO;

    // Camera control
    float m_lastX, m_lastY;
    bool m_firstMouse;
    float m_deltaTime, m_lastFrame;

    // Private methods
    bool initializeOpenGL();
    bool createShaders();
    void createPlaneMesh();
    void createCarMesh();
#ifdef HAVE_ASSIMP
    void loadCarModel();
#endif
    void setupInputCallbacks();

    // Static callbacks
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};
