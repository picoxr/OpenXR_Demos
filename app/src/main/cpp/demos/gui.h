/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include "guiBase.h"

class Gui {
public:
    Gui(std::string name);
    ~Gui();
    bool initialize(int32_t width, int32_t height);
    void render(const glm::mat4& p, const glm::mat4& v);
    void setModel(const glm::mat4& m);
    void getWidthHeight(float& width, float& height);
    bool isIntersectWithLine(const glm::vec3& linePoint, const glm::vec3& lineDirection);
    void active();
    void begin();
    void end();
    void triggerEvent(bool down);

private:
    bool initShader();
    void updateMousePosition(float x, float y);

private:
    static Shader mShader;
    std::string mName;

    GLuint mFramebuffer;
    GLuint mTextureColorbuffer;
    GLuint mVAO;
    GLuint mVBO;

    int32_t mWidth;
    int32_t mHeight;

    glm::mat4 mModel;
    glm::vec3 mIntersectionPoint;
};