#include "Mesh.h"
#include <iostream>

Mesh::Mesh() : m_VAO(0), m_VBO(0), m_EBO(0), m_initialized(false) {
}

Mesh::~Mesh() {
    cleanup();
}

void Mesh::createPlane(float width, float height, int subdivisions) {
    m_vertices.clear();
    m_indices.clear();
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // Create vertices for a subdivided plane
    for (int y = 0; y <= subdivisions; ++y) {
        for (int x = 0; x <= subdivisions; ++x) {
            float xPos = (float)x / subdivisions * width - halfWidth;
            float zPos = (float)y / subdivisions * height - halfHeight;
            
            Vertex vertex;
            vertex.position = glm::vec3(xPos, 0.0f, zPos);
            // Rotate texture coordinates 180 degrees clockwise (flip both U and V)
            vertex.texCoord = glm::vec2(1.0f - (float)x / subdivisions, (float)y / subdivisions);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            
            m_vertices.push_back(vertex);
        }
    }
    
    // Create indices
    for (int y = 0; y < subdivisions; ++y) {
        for (int x = 0; x < subdivisions; ++x) {
            int i = y * (subdivisions + 1) + x;
            
            // First triangle
            m_indices.push_back(i);
            m_indices.push_back(i + subdivisions + 1);
            m_indices.push_back(i + 1);
            
            // Second triangle
            m_indices.push_back(i + 1);
            m_indices.push_back(i + subdivisions + 1);
            m_indices.push_back(i + subdivisions + 2);
        }
    }
    
    setupMesh();
}

void Mesh::createGrid(float width, float height, int xDivisions, int yDivisions) {
    m_vertices.clear();
    m_indices.clear();
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // Create vertices
    for (int y = 0; y <= yDivisions; ++y) {
        for (int x = 0; x <= xDivisions; ++x) {
            float xPos = (float)x / xDivisions * width - halfWidth;
            float zPos = (float)y / yDivisions * height - halfHeight;
            
            Vertex vertex;
            vertex.position = glm::vec3(xPos, 0.0f, zPos);
            // Rotate texture coordinates 180 degrees clockwise (flip both U and V)
            vertex.texCoord = glm::vec2(1.0f - (float)x / xDivisions, (float)y / yDivisions);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            
            m_vertices.push_back(vertex);
        }
    }
    
    // Create indices
    for (int y = 0; y < yDivisions; ++y) {
        for (int x = 0; x < xDivisions; ++x) {
            int i = y * (xDivisions + 1) + x;
            
            // First triangle
            m_indices.push_back(i);
            m_indices.push_back(i + xDivisions + 1);
            m_indices.push_back(i + 1);
            
            // Second triangle
            m_indices.push_back(i + 1);
            m_indices.push_back(i + xDivisions + 1);
            m_indices.push_back(i + xDivisions + 2);
        }
    }
    
    setupMesh();
}

void Mesh::createCarModel(float length, float width, float height) {
    m_vertices.clear();
    m_indices.clear();
    
    float halfLength = length * 0.5f;
    float halfWidth = width * 0.5f;
    
    // Define vertices for a simple car box model
    // Car body (main box)
    std::vector<glm::vec3> positions = {
        // Bottom face (y = 0)
        {-halfLength, 0.0f, -halfWidth},  // 0: back-left
        { halfLength, 0.0f, -halfWidth},  // 1: front-left  
        { halfLength, 0.0f,  halfWidth},  // 2: front-right
        {-halfLength, 0.0f,  halfWidth},  // 3: back-right
        
        // Top face (y = height)
        {-halfLength, height, -halfWidth}, // 4: back-left
        { halfLength, height, -halfWidth}, // 5: front-left
        { halfLength, height,  halfWidth}, // 6: front-right 
        {-halfLength, height,  halfWidth}  // 7: back-right
    };
    
    // Create vertices with texture coordinates and normals
    for (size_t i = 0; i < positions.size(); ++i) {
        Vertex vertex;
        vertex.position = positions[i];
        vertex.texCoord = glm::vec2(0.5f, 0.5f); // Neutral texture coordinate
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Will be computed per face
        m_vertices.push_back(vertex);
    }
    
    // Define indices for the car box (12 triangles = 6 faces)
    std::vector<unsigned int> boxIndices = {
        // Bottom face (facing down)
        0, 2, 1,  0, 3, 2,
        // Top face (facing up) 
        4, 5, 6,  4, 6, 7,
        // Front face
        1, 6, 5,  1, 2, 6,
        // Back face
        3, 4, 7,  3, 0, 4,
        // Left face
        0, 5, 4,  0, 1, 5,
        // Right face
        2, 7, 6,  2, 3, 7
    };
    
    m_indices = boxIndices;
    
    // Calculate proper normals for each face
    for (size_t i = 0; i < m_indices.size(); i += 3) {
        unsigned int i0 = m_indices[i];
        unsigned int i1 = m_indices[i + 1];
        unsigned int i2 = m_indices[i + 2];
        
        glm::vec3 v0 = m_vertices[i0].position;
        glm::vec3 v1 = m_vertices[i1].position;
        glm::vec3 v2 = m_vertices[i2].position;
        
        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        
        m_vertices[i0].normal = normal;
        m_vertices[i1].normal = normal;
        m_vertices[i2].normal = normal;
    }
    
    setupMesh();
}

void Mesh::setupMesh() {
    // Generate buffers
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    
    // Bind VAO
    glBindVertexArray(m_VAO);
    
    // Load vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);
    
    // Load index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &m_indices[0], GL_STATIC_DRAW);
    
    // Set vertex attribute pointers
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinates
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    
    // Unbind VAO
    glBindVertexArray(0);
    
    m_initialized = true;
}

void Mesh::bind() const {
    if (m_initialized) {
        glBindVertexArray(m_VAO);
    }
}

void Mesh::unbind() const {
    glBindVertexArray(0);
}

void Mesh::render() const {
    if (m_initialized) {
        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Mesh::cleanup() {
    if (m_initialized) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
        m_initialized = false;
    }
}
