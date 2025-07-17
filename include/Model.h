#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <string>
#include <memory>

class Shader;

struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

struct ModelTexture {
    unsigned int id;
    std::string type;
    std::string path;
};

class ModelMesh {
public:
    std::vector<ModelVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<ModelTexture> textures;
    glm::vec3 diffuseColor;   // Material diffuse color
    glm::vec3 specularColor;  // Material specular color
    glm::vec3 ambientColor;   // Material ambient color

    ModelMesh(std::vector<ModelVertex> vertices, std::vector<unsigned int> indices, std::vector<ModelTexture> textures,
              glm::vec3 diffuse = glm::vec3(1.0f), glm::vec3 specular = glm::vec3(1.0f), glm::vec3 ambient = glm::vec3(1.0f));
    void draw(Shader& shader);

private:
    unsigned int VAO, VBO, EBO;
    void setupMesh();
};

class Model {
public:
    std::vector<ModelTexture> textures_loaded;
    std::vector<ModelMesh> meshes;
    std::string directory;

    Model(const std::string& path);
    void draw(Shader& shader);

private:
    const aiScene* scene = nullptr;  // Store scene reference for embedded textures
    
    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    ModelMesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<ModelTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
    unsigned int textureFromFile(const char* path, const std::string& directory, bool gamma = false);
    unsigned int loadEmbeddedTexture(const std::string& texPath);
    unsigned int createDefaultTexture();
};
