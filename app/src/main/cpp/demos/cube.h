/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <stdint.h>
#include <memory>
#include <openxr/openxr.h>
#include "common/gfxwrapper_opengl.h"
#include "shader.h"

class CubeRender {
public:
    CubeRender();
    ~CubeRender();
    bool initialize();
    struct Cube {
        glm::mat4 model;
        float scale;
    };
    void render(const glm::mat4& p, const glm::mat4& v, std::vector<Cube> &cubes);
private:
    bool initShader();
private:
    static Shader mShader;
    GLuint mFramebuffer;
    GLuint mCubeVertexBuffer;
    GLuint mCubeIndexBuffer;
    GLuint mVAO;
    GLuint mVBO;
};