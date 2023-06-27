/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "cube.h"
#include "utils.h"
#include "geometry.h"
#include "glm/gtc/matrix_transform.hpp"

Shader CubeRender::mShader;
CubeRender::CubeRender(): mFramebuffer(0), mVAO(0), mVBO(0) {
}
CubeRender::~CubeRender() {
}

bool CubeRender::initShader() {
    static bool init = false;
    if (init) {
        return true;
    }
    else {
        const GLchar* vertex_shader_glsl = R"_(
            #version 320 es
            precision highp float;
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 color;
            out vec3 fColor;
            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;
            void main()
            {
                gl_Position = projection * view * model* vec4(position, 1.0);
                fColor = color;
            }
        )_";

        const GLchar* fragment_shader_glsl = R"_(
            #version 320 es
            precision highp float;
            in vec3 fColor;
            out vec4 FragColor;
            void main()
            {
                FragColor = vec4(fColor, 1);
            }
        )_";

        if (mShader.loadShader(vertex_shader_glsl, fragment_shader_glsl) == false) {
            return false;
        }
        init = true;
    }
    return true;
}

bool CubeRender::initialize() {
    if (!initShader()) {
        return false;
    }

    //GL_CALL(glGenFramebuffers(1, &mFramebuffer));
    //GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));

    GL_CALL(glGenBuffers(1, &mCubeVertexBuffer));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mCubeVertexBuffer));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(Geometry::c_cubeVertices), Geometry::c_cubeVertices, GL_STATIC_DRAW));

    GL_CALL(glGenBuffers(1, &mCubeIndexBuffer));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mCubeIndexBuffer));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Geometry::c_cubeIndices), Geometry::c_cubeIndices, GL_STATIC_DRAW));


    GLint vertex_location_postion = glGetAttribLocation(mShader.id(), "position");
    GLint vertex_location_color = glGetAttribLocation(mShader.id(), "color");

    GL_CALL(glGenVertexArrays(1, &mVAO));
    GL_CALL(glBindVertexArray(mVAO));
    GL_CALL(glEnableVertexAttribArray(vertex_location_postion));
    GL_CALL(glEnableVertexAttribArray(vertex_location_color));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mCubeVertexBuffer));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mCubeIndexBuffer));
    GL_CALL(glVertexAttribPointer(vertex_location_postion, sizeof(XrVector3f) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr));
    GL_CALL(glVertexAttribPointer(vertex_location_color,   sizeof(XrVector3f) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), reinterpret_cast<const void*>(sizeof(XrVector3f))));

    return true;
}

void CubeRender::render(const glm::mat4& p, const glm::mat4& v, std::vector<Cube> &cubes) {
    mShader.use(); 
    mShader.setUniformMat4("projection", p);
    mShader.setUniformMat4("view", v);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    GL_CALL(glBindVertexArray(mVAO));
    for (const Cube& cube : cubes) {
        // Compute the model-view-projection transform and set it..
        //XrMatrix4x4f model;
        //XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
        //XrMatrix4x4f mvp;
        //XrMatrix4x4f_Multiply(&mvp, &vp, &model);
        //glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp));
        glm::mat4 model = glm::scale(cube.model, glm::vec3(cube.scale, cube.scale, cube.scale));
        mShader.setUniformMat4("model", model);

        // Draw the cube.
        GL_CALL(glDrawElements(GL_TRIANGLES, sizeof(Geometry::c_cubeIndices) / sizeof(Geometry::c_cubeIndices[0]), GL_UNSIGNED_SHORT, nullptr));
    }
    GL_CALL(glBindVertexArray(0));
}
