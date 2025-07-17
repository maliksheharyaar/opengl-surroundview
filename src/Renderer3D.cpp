#include "Renderer3D.h"
#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"
#ifdef HAVE_ASSIMP
#include "Model.h"
#endif
#include <iostream>
#include <fstream>

// Global pointer for callbacks
Renderer3D* g_renderer = nullptr;

Renderer3D::Renderer3D(int width, int height)
    : m_window(nullptr), m_width(width), m_height(height),
      m_textureID(0), m_VAO(0), m_VBO(0), m_EBO(0),
      m_lastX(width / 2.0f), m_lastY(height / 2.0f),
      m_firstMouse(true), m_deltaTime(0.0f), m_lastFrame(0.0f) {
    g_renderer = this;
}

Renderer3D::~Renderer3D() {
    cleanup();
}

bool Renderer3D::initialize() {
    if (!initializeOpenGL()) {
        return false;
    }

    // Initialize camera - position it above the model for optimal view, rotated 180° clockwise
    m_camera = std::make_unique<Camera>(glm::vec3(0.0f, 25.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 90.0f, -90.0f);

    // Create shaders
    if (!createShaders()) {
        return false;
    }

    // Create plane mesh for rendering the image
    createPlaneMesh();

    // Create car mesh for the 3D car model
    createCarMesh();

#ifdef HAVE_ASSIMP
    // Load GLB car model if Assimp is available
    loadCarModel();
#endif

    // Generate texture
    glGenTextures(1, &m_textureID);

    // Setup input callbacks
    setupInputCallbacks();

    std::cout << "Renderer initialized successfully." << std::endl;
    return true;
}

bool Renderer3D::initializeOpenGL() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    m_window = glfwCreateWindow(m_width, m_height, "SurroundView3D", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    // Configure OpenGL
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, m_width, m_height);

    return true;
}

bool Renderer3D::createShaders() {
    m_shader = std::make_unique<Shader>();

    std::string vertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // Scale texture coordinates to ensure we see the full image
    TexCoord = aTexCoord * 1.0; // You can adjust this scale factor
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

    std::string fragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D imageTexture;
uniform sampler2D texture_diffuse1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int useTexture;
uniform vec3 solidColor;
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform vec3 materialAmbient;

void main() {
    vec3 color;
    
    if (useTexture == 1) {
        // Use image texture (for the plane) - no lighting adjustment needed
        color = texture(imageTexture, TexCoord).rgb;
        FragColor = vec4(color, 1.0);
    } else if (useTexture == 2) {
        // Use GLB model diffuse texture
        color = texture(texture_diffuse1, TexCoord).rgb;
        
        // Add proper directional lighting for 3D model
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * color;
        
        vec3 ambient = 0.7 * color;
        vec3 result = ambient + diffuse;
        FragColor = vec4(result, 1.0);
    } else {
        // Use material colors from GLB file
        color = materialDiffuse;
        
        // Add proper directional lighting for 3D model
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * color;
        
        vec3 ambient = 0.7 * color;
        vec3 result = ambient + diffuse;
        FragColor = vec4(result, 1.0);
    }
}
)";

    if (!m_shader->loadFromStrings(vertexShader, fragmentShader)) {
        std::cerr << "Failed to create shaders" << std::endl;
        return false;
    }

    return true;
}

void Renderer3D::createPlaneMesh() {
    m_planeMesh = std::make_unique<Mesh>();
    
    // Calculate aspect ratio for surround view texture (2280x2240)
    float textureWidth = 2280.0f;
    float textureHeight = 2240.0f;
    float aspectRatio = textureWidth / textureHeight; // ~1.018
    
    // Create plane with correct aspect ratio to prevent compression
    float planeBaseSize = 50.0f;
    float planeWidth = planeBaseSize * aspectRatio;  // ~50.9 units wide
    float planeHeight = planeBaseSize;               // 50.0 units tall
    
    m_planeMesh->createPlane(planeWidth, planeHeight, 1);
}

void Renderer3D::createCarMesh() {
    m_carMesh = std::make_unique<Mesh>();
    m_carMesh->createCarModel(4.5f, 2.0f, 1.5f); // Realistic car dimensions (length, width, height)
}

void Renderer3D::render() {
    static int frameCount = 0;
    frameCount++;
    
    float currentFrame = glfwGetTime();
    m_deltaTime = currentFrame - m_lastFrame;
    m_lastFrame = currentFrame;

    // Maintain fixed camera position and orientation
    m_camera->maintainTopDownView();

    // Clear buffers
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use shader
    m_shader->use();

    // Set matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = m_camera->getViewMatrix();
    glm::mat4 projection = m_camera->getProjectionMatrix((float)m_width / (float)m_height);

    m_shader->setMat4("model", model);
    m_shader->setMat4("view", view);
    m_shader->setMat4("projection", projection);

    // Set lighting uniforms - position light above and in front for better visibility
    m_shader->setVec3("lightPos", glm::vec3(0.0f, 30.0f, 10.0f));
    m_shader->setVec3("viewPos", m_camera->getPosition());
    
    // Enable texture by default
    m_shader->setInt("useTexture", 1);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    m_shader->setInt("imageTexture", 0);

    // Render plane mesh (ground/image)
    m_planeMesh->render();

    // Render car model (centered in the view)
    // Position the car at the center of the surround view
    glm::mat4 carModel = glm::mat4(1.0f);
    carModel = glm::translate(carModel, glm::vec3(0.0f, 0.1f, 0.0f)); // Center position, slightly elevated
    carModel = glm::rotate(carModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate 180 degrees clockwise
    carModel = glm::scale(carModel, glm::vec3(0.01f, 0.01f, 0.01f)); // FURTHER REDUCED scale for smaller car model
    m_shader->setMat4("model", carModel);
    
#ifdef HAVE_ASSIMP
    if (m_carModel) {
        // Use GLB model if loaded successfully - let Model class handle all shader settings
        m_carModel->draw(*m_shader);
    }
    // Remove fallback car mesh to avoid double rendering
#endif
    
    // Re-enable texture for future renders
    m_shader->setInt("useTexture", 1);

    m_shader->unuse();
}

void Renderer3D::updateTexture(const cv::Mat& image) {
    if (image.empty()) {
        std::cout << "Warning: Empty image passed to updateTexture" << std::endl;
        return;
    }

    // Remove the flip to correct the orientation
    std::cout << "Updating texture - Size: " << image.cols << "x" << image.rows 
              << ", Channels: " << image.channels() << std::endl;

    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    // Convert BGR to RGB for OpenGL
    cv::Mat rgbImage;
    if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    } else {
        rgbImage = image;
    }
    
    GLenum format = GL_RGB;
    if (rgbImage.channels() == 1) format = GL_LUMINANCE;
    else if (rgbImage.channels() == 3) format = GL_RGB;
    else if (rgbImage.channels() == 4) format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 
                 0, format, GL_UNSIGNED_BYTE, rgbImage.data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    std::cout << "Texture updated successfully" << std::endl;
}

void Renderer3D::updateCamera(float deltaTime) {
    // Removed keyboard input handling - only zoom allowed via scroll wheel
    // Camera position is fixed directly above the model
}

void Renderer3D::setupInputCallbacks() {
    // Only set scroll callback for zoom functionality
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    // Remove mouse cursor callback and keep cursor visible
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool Renderer3D::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Renderer3D::swapBuffers() {
    glfwSwapBuffers(m_window);
}

void Renderer3D::pollEvents() {
    glfwPollEvents();
}

void Renderer3D::cleanup() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

#ifdef HAVE_ASSIMP
void Renderer3D::loadCarModel() {
    try {
        // Try GLB first (prioritize GLB over FBX)
        if (std::ifstream("assets/model.glb").good()) {
            m_carModel = std::make_unique<Model>("assets/model.glb");
            std::cout << "GLB car model loaded successfully!" << std::endl;
        } else {
            std::cerr << "GLB model file not found (assets/model.glb)" << std::endl;
            m_carModel = nullptr;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load car model: " << e.what() << std::endl;
        std::cerr << "Falling back to simple car mesh" << std::endl;
        m_carModel = nullptr;
    }
}
#endif

// Static callback functions
void Renderer3D::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    // Mouse movement disabled - no camera rotation allowed
}

void Renderer3D::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_renderer) {
        // Only allow zoom in/out via FOV adjustment
        g_renderer->m_camera->processMouseScroll(yoffset);
    }
}

void Renderer3D::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}
