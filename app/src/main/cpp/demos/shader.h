/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <string>
#include "glm/glm.hpp"
#include "common/gfxwrapper_opengl.h"

class Shader {
public:
    Shader();
    ~Shader();

    bool loadShader(const char* vertexCode, const char* fragmentCode);

    void use() const;
    GLuint id() const;
    void setUniformBool(const std::string& name, bool value) const;
    void setUniformInt(const std::string& name, int value) const;
    void setUniformFloat(const std::string& name, float value) const;
    void setUniformVec2(const std::string& name, const glm::vec2& value) const;
    void setUniformVec2(const std::string& name, float x, float y) const;
    void setUniformVec3(const std::string& name, const glm::vec3& value) const;
    void setUniformVec3(const std::string& name, float x, float y, float z) const;
    void setUniformVec4(const std::string& name, const glm::vec4& value) const;
    void setUniformVec4(const std::string& name, float x, float y, float z, float w) const;
    void setUniformMat2(const std::string& name, const glm::mat2& mat) const;
    void setUniformMat3(const std::string& name, const glm::mat3& mat) const;
    void setUniformMat4(const std::string& name, const glm::mat4& mat) const;
    GLuint getAttribLocation(const std::string& name) const;
private:
    bool checkCompileErrors(GLuint shader, std::string type);

private:
    GLuint mProgram;
};