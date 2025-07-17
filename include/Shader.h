#pragma once

#include <GL/glew.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader();
    ~Shader();

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool loadFromStrings(const std::string& vertexSource, const std::string& fragmentSource);
    
    void use() const;
    void unuse() const;

    // Uniform setters
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

    GLuint getProgram() const { return m_program; }
    GLuint getID() const { return m_program; }

private:
    GLuint m_program;

    bool compileShader(GLuint shader, const std::string& source);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::string readFile(const std::string& filepath);
};
