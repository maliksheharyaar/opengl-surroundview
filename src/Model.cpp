#include "Model.h"
#include "Shader.h"
#include <iostream>
#include <opencv2/opencv.hpp>

// ModelMesh implementation
ModelMesh::ModelMesh(std::vector<ModelVertex> vertices, std::vector<unsigned int> indices, std::vector<ModelTexture> textures,
                     glm::vec3 diffuse, glm::vec3 specular, glm::vec3 ambient) {
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;
    this->diffuseColor = diffuse;
    this->specularColor = specular;
    this->ambientColor = ambient;

    setupMesh();
}

void ModelMesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ModelVertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)0);
    // vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, texCoords));

    glBindVertexArray(0);
}

void ModelMesh::draw(Shader& shader) {
    // Bind appropriate textures if available
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;
    
    // If we have textures, use them
    if (!textures.empty()) {
        shader.setInt("useTexture", 2); // Signal to use GLB model texture
        
        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i + 1); // Start from texture unit 1
            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);
            else if (name == "texture_normal")
                number = std::to_string(normalNr++);
            else if (name == "texture_height")
                number = std::to_string(heightNr++);

            glUniform1i(glGetUniformLocation(shader.getID(), (name + number).c_str()), i + 1);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
    } else {
        // No textures available, use material colors
        shader.setInt("useTexture", 0);
        shader.setVec3("materialDiffuse", diffuseColor);
        shader.setVec3("materialSpecular", specularColor);
        shader.setVec3("materialAmbient", ambientColor);
    }
    
    // draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset to texture unit 0
    glActiveTexture(GL_TEXTURE0);
}

// Model implementation
Model::Model(const std::string& path) {
    loadModel(path);
}

void Model::draw(Shader& shader) {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].draw(shader);
}

void Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    
    // Use more comprehensive import flags for better texture and model loading
    unsigned int flags = aiProcess_Triangulate | 
                        aiProcess_GenSmoothNormals | 
                        aiProcess_FlipUVs | 
                        aiProcess_CalcTangentSpace |
                        aiProcess_JoinIdenticalVertices |
                        aiProcess_SortByPType |
                        aiProcess_FindInstances |
                        aiProcess_ValidateDataStructure;
    
    const aiScene* scene = importer.ReadFile(path, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return;
    }

    // Store scene reference for embedded texture access
    this->scene = scene;
    directory = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene);
    
    std::cout << "Model loaded successfully: " << path << std::endl;
    std::cout << "Materials: " << scene->mNumMaterials << std::endl;
    std::cout << "Meshes: " << meshes.size() << std::endl;
    std::cout << "Embedded textures: " << scene->mNumTextures << std::endl;
    
    // Debug: Print embedded texture information
    for (unsigned int i = 0; i < scene->mNumTextures; i++) {
        aiTexture* texture = scene->mTextures[i];
        std::cout << "Embedded texture " << i << ": ";
        if (texture->mHeight == 0) {
            std::cout << "Compressed format, size: " << texture->mWidth << " bytes" << std::endl;
        } else {
            std::cout << "Raw format, dimensions: " << texture->mWidth << "x" << texture->mHeight << std::endl;
        }
    }
    
    // Debug: Print material and texture information
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        std::cout << "Material " << i << ":" << std::endl;
        std::cout << "  Diffuse textures: " << mat->GetTextureCount(aiTextureType_DIFFUSE) << std::endl;
        std::cout << "  Specular textures: " << mat->GetTextureCount(aiTextureType_SPECULAR) << std::endl;
        std::cout << "  Normal textures: " << mat->GetTextureCount(aiTextureType_NORMALS) << std::endl;
    }
    
    // Debug: Print texture information per mesh
    for (size_t i = 0; i < meshes.size(); i++) {
        std::cout << "Mesh " << i << " has " << meshes[i].textures.size() << " textures" << std::endl;
        for (size_t j = 0; j < meshes[i].textures.size(); j++) {
            std::cout << "  Texture " << j << ": " << meshes[i].textures[j].type << " - " << meshes[i].textures[j].path << std::endl;
        }
    }
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

ModelMesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<ModelVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<ModelTexture> textures;

    // walk through each of the mesh's vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        ModelVertex vertex;
        glm::vec3 vector;
        
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;
        
        // normals
        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
        }
        
        // texture coordinates
        if (mesh->mTextureCoords[0]) {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.texCoords = vec;
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }
    
    // now walk through each of the mesh's faces
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    
    // process materials
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    
    // Load different types of textures
    std::vector<ModelTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    
    std::vector<ModelTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    
    std::vector<ModelTexture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    
    std::vector<ModelTexture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    
    // Also try to load base color textures (common in PBR materials)
    std::vector<ModelTexture> baseColorMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse");
    textures.insert(textures.end(), baseColorMaps.begin(), baseColorMaps.end());
    
    // Try to load unknown textures as diffuse
    std::vector<ModelTexture> unknownMaps = loadMaterialTextures(material, aiTextureType_UNKNOWN, "texture_diffuse");
    textures.insert(textures.end(), unknownMaps.begin(), unknownMaps.end());

    // Debug: Print material color properties
    aiColor3D diffuseColor, specularColor, ambientColor;
    material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
    material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
    material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor);
    
    std::cout << "Material " << mesh->mMaterialIndex << " colors - "
              << "Diffuse: (" << diffuseColor.r << ", " << diffuseColor.g << ", " << diffuseColor.b << "), "
              << "Specular: (" << specularColor.r << ", " << specularColor.g << ", " << specularColor.b << "), "
              << "Ambient: (" << ambientColor.r << ", " << ambientColor.g << ", " << ambientColor.b << ")" << std::endl;

    return ModelMesh(vertices, indices, textures, 
                    glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b),
                    glm::vec3(specularColor.r, specularColor.g, specularColor.b),
                    glm::vec3(ambientColor.r, ambientColor.g, ambientColor.b));
}

std::vector<ModelTexture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
    std::vector<ModelTexture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        if (!skip) {
            ModelTexture texture;
            texture.id = textureFromFile(str.C_Str(), this->directory);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    return textures;
}

unsigned int Model::textureFromFile(const char* path, const std::string& directory, bool gamma) {
    std::string filename = std::string(path);
    
    // Check if this is an embedded texture (starts with *)
    if (filename[0] == '*') {
        // This is an embedded texture, handle it differently
        std::cout << "Loading embedded texture: " << filename << std::endl;
        return loadEmbeddedTexture(filename);
    }
    
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);
    if (!image.empty()) {
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        std::cout << "Loaded external texture: " << filename << std::endl;
    } else {
        std::cout << "Texture failed to load at path: " << filename << std::endl;
        // Create a default texture instead of returning invalid texture
        glDeleteTextures(1, &textureID);
        return createDefaultTexture();
    }

    return textureID;
}

unsigned int Model::createDefaultTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    // Create a 1x1 white texture
    unsigned char whitePixel[3] = {255, 255, 255}; // Pure white
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    std::cout << "Created default white texture with ID: " << textureID << std::endl;
    return textureID;
}

unsigned int Model::loadEmbeddedTexture(const std::string& texPath) {
    // Extract texture index from path (format: "*0", "*1", etc.)
    int textureIndex = std::stoi(texPath.substr(1));
    
    if (!scene || textureIndex >= (int)scene->mNumTextures) {
        std::cout << "Invalid embedded texture index: " << textureIndex << std::endl;
        return createDefaultTexture();
    }
    
    aiTexture* texture = scene->mTextures[textureIndex];
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    if (texture->mHeight == 0) {
        // Compressed texture (PNG, JPG, etc.)
        cv::Mat image = cv::imdecode(cv::Mat(1, texture->mWidth, CV_8UC1, texture->pcData), cv::IMREAD_COLOR);
        
        if (!image.empty()) {
            cv::Mat rgbImage;
            cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
            
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
            glGenerateMipmap(GL_TEXTURE_2D);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            std::cout << "Loaded embedded compressed texture " << textureIndex << " (" << rgbImage.cols << "x" << rgbImage.rows << ")" << std::endl;
        } else {
            std::cout << "Failed to decode embedded texture " << textureIndex << std::endl;
            glDeleteTextures(1, &textureID);
            return createDefaultTexture();
        }
    } else {
        // Raw texture data
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Assume RGBA format for raw embedded textures
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->mWidth, texture->mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pcData);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        std::cout << "Loaded embedded raw texture " << textureIndex << " (" << texture->mWidth << "x" << texture->mHeight << ")" << std::endl;
    }
    
    return textureID;
}
