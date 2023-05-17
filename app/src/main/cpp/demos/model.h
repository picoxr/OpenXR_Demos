/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <string>
#include <map>
#include <vector>
#include "mesh.h"
#include "shader.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "shader.h"

class Model {
public:
    Model() = delete;
    Model(const std::string& name);
    ~Model();

    std::string& name();

    bool loadModel(const std::string& modelFileName);

    bool initialize() { return false; };

    bool bindMeshTexture(const std::string& meshName, const std::string& textureName);
    bool activeMeshTexture(const std::string& meshName, const std::string& textureName);

    bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m);

private:
    void initShader();
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
    std::vector<Texture> loadMaterialTextures_force(aiMaterial* mat, aiTextureType type, std::string typeName, std::string file);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    void draw();

private:
    std::string mName;
    std::map<std::string, Mesh> mMeshes;
    bool mIsGammaCorrection;

    std::vector<Texture> mTexturesLoaded;
    std::string mDirectory;

    std::map<std::string, std::vector<std::string>> mMeshTexturesMap;

    static Shader mShader;
};