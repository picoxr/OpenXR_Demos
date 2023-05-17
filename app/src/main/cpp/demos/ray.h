/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <vector>
#include "shader.h"
#include "glm/glm.hpp"
#include "common/gfxwrapper_opengl.h"

#define PI 3.1415926535
#define RADIAN(x) ((x) * PI / 180)

class Ray {
public:
    Ray();
    ~Ray();
    void initialize();
    bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m);
    std::vector<glm::vec3> getPoints();
    glm::vec3 getForwardVector();
    glm::vec3 getDirectionVector(const glm::mat4& m);
    void setColor(const glm::vec3& color);
    void setColor(float x, float y, float z);
private:
    bool initShader();
private:
    static Shader mShader;
    float mRadius = 0.0015f;
    float mLength = 2.0f;
    uint32_t mVertexCount;
    glm::vec3 mPoint1;
    glm::vec3 mPoint2;
    std::vector<float> mVertices;
    std::vector<GLuint> mIndices;
    GLuint mVAO;
    glm::vec3 mColor;
};