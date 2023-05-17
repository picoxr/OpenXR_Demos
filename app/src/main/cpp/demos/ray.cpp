/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "ray.h"
#include "utils.h"

Shader Ray::mShader;
Ray::Ray() {
    mColor = {1.0f, 1.0f, 1.0f};
}

Ray::~Ray() {
}

bool Ray::initShader() {
    static bool init = false;
    if (init) {
        return true;
    }
    else {
        const GLchar* vertex_shader_glsl = R"_(
            #version 320 es
            precision highp float;
            layout (location = 0) in vec3 position;
            uniform mat4 projection;
            uniform mat4 view;
            uniform mat4 model;
            out vec3 outPosition;
            void main()
            {
                outPosition = position;
                gl_Position = projection * view * model * vec4(position, 1.0);
            }
        )_";

        const GLchar* fragment_shader_glsl = R"_(
            #version 320 es
            precision mediump float;
            uniform float inmaxz;
            uniform vec3 color;
            in vec3 outPosition;
            out vec4 FragColor;
            void main()
            {
                float t = 1.0f;
                if (outPosition.z < inmaxz * 0.5f) {
                   t = 1.0f + (inmaxz * 0.5f - outPosition.z) / (inmaxz * 0.5f);
                }
                FragColor = vec4(color, t);
            }
        )_";

        if (mShader.loadShader(vertex_shader_glsl, fragment_shader_glsl) == false) {
            return false;
        }
        init = true;
    }
    return true;
}

void Ray::initialize() {
    initShader();
    float angle_span = 20;
    float startz = -0.05f;
    mVertexCount = 0;
    for (float angle = 0; angle <= 360; angle += angle_span) {
        float x = (float) mRadius * sin(RADIAN(angle));
        float y = (float) mRadius * cos(RADIAN(angle));
        float z = startz;

        mVertices.push_back(x);
        mVertices.push_back(y);
        mVertices.push_back(z);
        mVertexCount++;

        mVertices.push_back(x);
        mVertices.push_back(y);
        mVertices.push_back(z - mLength);
        mVertexCount++;

        if (mVertexCount >= 4) {
            mIndices.push_back(mVertexCount - 4);
            mIndices.push_back(mVertexCount - 3);
            mIndices.push_back(mVertexCount - 2);
            mIndices.push_back(mVertexCount - 3);
            mIndices.push_back(mVertexCount - 1);
            mIndices.push_back(mVertexCount - 2);
        }
    }
    mPoint1 = glm::vec3(0.0f, 0.0f, startz);              //start point
    mPoint2 = glm::vec3(0.0f, 0.0f, startz - mLength);    //end point        use to calculate line direction

    GLuint VBO = 0, EBO = 0;
	GL_CALL(glGenVertexArrays(1, &mVAO));
	GL_CALL(glGenBuffers(1, &VBO));
    GL_CALL(glGenBuffers(1, &EBO));
	GL_CALL(glBindVertexArray(mVAO));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
	GL_CALL(glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(GLuint), mIndices.data(), GL_STATIC_DRAW));
	GL_CALL(glEnableVertexAttribArray(0));
	GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0));
}

glm::vec3 Ray::getForwardVector() {
    return glm::normalize(glm::vec3(mPoint2 - mPoint1));
}

glm::vec3 Ray::getDirectionVector(const glm::mat4& m) {
    glm::vec3 point2 = glm::vec3(m * glm::vec4(mPoint2, 1.0f));
    glm::vec3 point1 = glm::vec3(m * glm::vec4(mPoint1, 1.0f));
    return glm::normalize(point2 - point1);
}

void Ray::setColor(const glm::vec3& color) {
    mColor = color;
}

void Ray::setColor(float x, float y, float z) {
    mColor = {x, y, z};
}

bool Ray::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) {
    //GL_CALL(glDisable(GL_CULL_FACE));
    mShader.use();
    mShader.setUniformVec3("color", mColor);
    mShader.setUniformMat4("projection", p);
    mShader.setUniformMat4("view", v);
    mShader.setUniformMat4("model", m);
    float maxz = mVertices[mVertices.size() - 1];
    mShader.setUniformFloat("inmaxz", maxz);
    GL_CALL(glBindVertexArray(mVAO));
    GL_CALL(glDrawElements(GL_TRIANGLES, mIndices.size(), GL_UNSIGNED_INT, 0));
    GL_CALL(glBindVertexArray(0));
    return true;
}

std::vector<glm::vec3> Ray::getPoints() {
    std::vector<glm::vec3> points;
    points.push_back(mPoint1);
    points.push_back(mPoint2);
    return points;
}
