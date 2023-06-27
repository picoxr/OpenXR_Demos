/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "model.h"
#include "utils.h"
#include "logger.h"

Shader Model::mShader;
void Model::initShader() {
    static bool init = false;
    if (init) {
        return;
    } else {
        const char* vertexShaderCode = R"_(
            #version 320 es
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoords;
            layout(location = 3) in vec3 tangent;
            layout(location = 4) in vec3 bitangent;
            layout(location = 5) in ivec4 boneIds;
            layout(location = 6) in vec4 weights;
            
            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;

            const int MAX_BONE_NODES = 100;
            const int MAX_BONE_INFLUENCE = 4;
            uniform mat4 finalBoneNodesMatrices[MAX_BONE_NODES];

            out vec2 TexCoords;

            void main()
            {
                vec4 total_position = vec4(0.0f);
                bool has_bone = false;
                for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                    if (boneIds[i] < 0) {
                        continue;
                    }
                    if (boneIds[i] >= MAX_BONE_NODES) {
                        total_position = vec4(aPos, 1.0f);
                        break;
                    }
                    vec4 local_position = finalBoneNodesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
                    total_position += local_position * weights[i];
                    has_bone = true;
                }
                if (has_bone == false) {
                    total_position = vec4(aPos, 1.0f);
                }
                gl_Position = projection * view * model * total_position;
                TexCoords = aTexCoords;
            }
        )_";

        const char* fragmentShaderCode = R"_(
            #version 320 es
            precision mediump float;
            out vec4 FragColor;
            in vec2 TexCoords;
            uniform sampler2D texture_diffuse1;
            void main()
            {
                FragColor = texture(texture_diffuse1, TexCoords);
            }
        )_";
        mShader.loadShader(vertexShaderCode, fragmentShaderCode);
        init = true;
    }
}

Model::Model(const std::string& name, bool hasBoneInfo) : mName(name), mHasBoneInfo(hasBoneInfo) {
    mBoneInfoMap.clear();
}

Model::~Model() {
    mBoneInfoMap.clear();
}

std::string& Model::name() {
    return mName;
}

bool Model::bindMeshTexture(const std::string& meshName, const std::string& textureName) {
    auto it = mMeshTexturesMap.find(meshName);
    if (it == mMeshTexturesMap.end()) {
        std::vector<std::string> textureNames(1, textureName);
        mMeshTexturesMap[meshName] = textureNames;
    } else {
        it->second.push_back(textureName);
    }
    return true;
}

bool Model::activeMeshTexture(const std::string& meshName, const std::string& textureName) {
    auto it = mMeshes.find(meshName);
    if (it != mMeshes.end()) {
        return it->second.activeTexture(textureName);
    }
    return false;
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
    std::vector<Texture> textures;
    for (uint32_t i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        bool skip = false;
        for (uint32_t j = 0; j < mTexturesLoaded.size(); j++) {
            if (std::strcmp(mTexturesLoaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(mTexturesLoaded[j]);
                skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip) {
            Texture texture;
            texture.id = TextureFromFileAssets(str.C_Str(), mDirectory);
            texture.type = typeName;
            texture.path = str.C_Str();
            texture.active = false;
            textures.push_back(texture);
            mTexturesLoaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
        }
    }
    return textures;
}

std::vector<Texture> Model::loadMaterialTextures_force(aiMaterial* mat, aiTextureType type, std::string typeName, std::string file) {
    std::vector<Texture> textures;
    bool skip = false;
    for (uint32_t j = 0; j < mTexturesLoaded.size(); j++) {
        if (std::strcmp(mTexturesLoaded[j].path.data(), file.c_str()) == 0) {
            skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
            break;
        }
    }
    if (!skip) {
        Texture texture;
        texture.id = TextureFromFileAssets(file.c_str(), "");
        texture.type = typeName;
        texture.path = file.c_str();
        texture.active = false;
        textures.push_back(texture);
        mTexturesLoaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
    }
    return textures;
}

void Model::processMeshBone(aiMesh* mesh, std::vector<Vertex>& vertices) {
    int boneIndex = 0;
    infof("processMeshBone mesh name:%s, vertices:%d, total bone:%d", mesh->mName.C_Str(), vertices.size(), mesh->mNumBones);
    for (uint32_t i = 0; i < mesh->mNumBones; i++) {
        infof("i:%02d, bone: %-16s, %02d, total weights:%d", i, mesh->mBones[i]->mName.C_Str(), boneIndex, mesh->mBones[i]->mNumWeights);
        std::string name = mesh->mBones[i]->mName.C_Str();
        std::shared_ptr<boneInfo> boneInformation(new boneInfo(boneIndex));
        auto it = mBoneInfoMap.find(name);
        if (it == mBoneInfoMap.end()) {
            mBoneInfoMap[name] = boneInformation;
        } else {
            errorf("already has boneNode %s", name.c_str());
        }

        for (int weightIndex = 0; weightIndex < mesh->mBones[i]->mNumWeights; weightIndex++) {
            int vertexIndex = mesh->mBones[i]->mWeights[weightIndex].mVertexId;
            float weight =  mesh->mBones[i]->mWeights[weightIndex].mWeight;
            if (weightIndex < vertices.size()) {
                Vertex &v = vertices[vertexIndex];
                int k = 0;
                for (k = 0; k < MAX_BONE_INFLUENCE; k++) {
                    if (v.BoneIDs[k] < 0) {
                        v.Weights[k] = weight;
                        v.BoneIDs[k] = boneIndex;
                        break;
                    }
                }
                if (k >= 4) {
                    //errorf("k >= 4, weightIndex:%d", weightIndex);
                }
            } else {
                errorf("weightIndex %d > vertices.size() %d", weightIndex, vertices.size());
            }

        }
        boneIndex++;
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    
    infof("mesh vertex count: %d, face count:%d, bone:%d", mesh->mNumVertices, mesh->mNumFaces, mesh->mNumBones);
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex{};
        glm::vec3 vector{};
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }

        if (mesh->mTextureCoords[0]) {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
            // tangent
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.Tangent = vector;
            // bitangent
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.Bitangent = vector;
        } else {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (mHasBoneInfo) {
        processMeshBone(mesh, vertices);
    }

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    /*
    // 1. diffuse maps
    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    */

    auto it = mMeshTexturesMap.find(mesh->mName.C_Str());
    if (it != mMeshTexturesMap.end()) {
        for (auto& i : it->second) {
            std::vector<Texture> texture = loadMaterialTextures_force(material, aiTextureType_DIFFUSE, "texture_diffuse", i);
            textures.insert(textures.end(), texture.begin(), texture.end());
        }
    }

    return Mesh(vertices, indices, textures);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    static std::string indent = "";
    infof("%snode:%s, children:%d", indent.c_str(), node->mName.C_Str(), node->mNumChildren);
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        infof("%smesh: %s", indent.c_str(), mesh->mName.C_Str());
        mMeshes.insert(std::pair<std::string, Mesh>(mesh->mName.C_Str(), processMesh(mesh, scene)));
    }
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        std::string tmp = indent;
        indent += "  ";
        processNode(node->mChildren[i], scene);
        indent = tmp;
    }
}

bool Model::loadModel(const std::string& modelFileName) {
    initShader();
    std::vector<char> fileData = readFileFromAssets(modelFileName.c_str());
    Assimp::Importer importer;
    //const aiScene* scene = importer.ReadFile(modelFileName, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    const aiScene* scene = importer.ReadFileFromMemory(fileData.data(), fileData.size(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
        Log::Write(Log::Level::Error, Fmt("assimp readfile error %s", importer.GetErrorString()));
        return false;
    }

    mDirectory = modelFileName.substr(0, modelFileName.find_last_of('/'));

    infof("model:%s, scene:%s, mNumMeshes:%d, mNumMaterials:%d, mNumAnimations:%d, mNumTextures:%d", modelFileName.c_str(), 
        scene->mName.C_Str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures);
    processNode(scene->mRootNode, scene);
    initializeBoneNode();
    return true;
}

void Model::draw() {
    GL_CALL(glFrontFace(GL_CCW));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_CULL_FACE));
    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    for (auto &it : mMeshes) {
        it.second.draw(mShader);
    }
}

bool Model::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) {
    mShader.use();
    mShader.setUniformMat4("projection", p);
    mShader.setUniformMat4("view", v);
    mShader.setUniformMat4("model", m);
    draw();
    glUseProgram(0);
    return true;
}

void Model::initializeBoneNode() {
    mShader.use();
    glm::mat4 m = glm::mat4(1.0f);
    for (auto it : mBoneInfoMap) {
        std::string name = "finalBoneNodesMatrices[" + std::to_string(it.second->id) +  "]";
        mShader.setUniformMat4(name, m);
    }
}

int Model::getBoneNodeIndexByName(const std::string& name) const {
    auto it = mBoneInfoMap.find(name);
    if (it != mBoneInfoMap.end()) {
        return it->second->id;
    }
    errorf("not found bone %s", name.c_str());
    return -1;
}

void Model::setBoneNodeMatrices(const std::string& bone, const glm::mat4& m) {
    int index = getBoneNodeIndexByName(bone);
    if (index < 0) {
        return;
    }
    std::string name = "finalBoneNodesMatrices[" + std::to_string(index) +  "]";
    mShader.use();
    mShader.setUniformMat4(name, m);
}
