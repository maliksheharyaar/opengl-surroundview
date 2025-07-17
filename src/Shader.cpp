#include "Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

Shader::Shader() : m_program(0) {
}

Shader::~Shader() {
    if (m_program) {
        glDeleteProgram(m_program);
    }
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    // Read vertex shader source
    std::ifstream vertexFile;
    std::ifstream fragmentFile;
    std::string vertexCode;
    std::string fragmentCode;
    
    vertexFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fragmentFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try {
        vertexFile.open(vertexPath);
        fragmentFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        
        vShaderStream << vertexFile.rdbuf();
        fShaderStream << fragmentFile.rdbuf();
        
        vertexFile.close();
        fragmentFile.close();
        
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        return false;
    }
    
    return loadFromStrings(vertexCode, fragmentCode);
}

bool Shader::loadFromStrings(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertex, fragment;
    
    // Vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    const char* vShaderCode = vertexSource.c_str();
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    if (!compileShader(vertex, vertexSource)) {
        glDeleteShader(vertex);
        return false;
    }
    
    // Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fShaderCode = fragmentSource.c_str();
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    if (!compileShader(fragment, fragmentSource)) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }
    
    // Shader Program
    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);
    
    // Check for linking errors
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_program, 1024, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    
    // Delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    return true;
}

void Shader::use() const {
    if (m_program) {
        glUseProgram(m_program);
    }
}

void Shader::unuse() const {
    glUseProgram(0);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(m_program, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(m_program, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(m_program, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(m_program, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

bool Shader::compileShader(GLuint shader, const std::string& source) {
    glCompileShader(shader);
    
    GLint success;
    GLchar infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    return true;
}
