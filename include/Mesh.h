#pragma once

#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
};

class Mesh {
public:
    Mesh();
    ~Mesh();

    void createPlane(float width = 10.0f, float height = 10.0f, int subdivisions = 1);
    void createGrid(float width = 10.0f, float height = 10.0f, int xDivisions = 10, int yDivisions = 10);
    void createCarModel(float length = 4.5f, float width = 2.0f, float height = 1.5f);
    
    void bind() const;
    void unbind() const;
    void render() const;

    size_t getVertexCount() const { return m_vertices.size(); }
    size_t getIndexCount() const { return m_indices.size(); }

private:
    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;

    GLuint m_VAO, m_VBO, m_EBO;
    bool m_initialized;

    void setupMesh();
    void cleanup();
};
