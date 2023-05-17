/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <stdint.h>
#include <memory>
#include "imgui/imgui.h"
#include "common/gfxwrapper_opengl.h"
#include "shader.h"

class GuiBase {
public:
    static GuiBase& instance();
    ~GuiBase();
    bool initialize();
    void render();
private:
    GuiBase();
    void initShader();
    void setupRenderState(ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object);
    bool renderDrawData(ImDrawData* draw_data);
private:
    ImGuiContext* mImguiContext;
    GLuint mFontTexture;
    GLuint mShaderHandle;
    GLint  mAttribLocationTex;       // Uniforms location
    GLint  mAttribLocationProjMtx;
    GLuint mAttribLocationVtxPos;    // Vertex attributes location
    GLuint mAttribLocationVtxUV;
    GLuint mAttribLocationVtxColor;
    uint32_t mVboHandle, mElementsHandle;
    GLsizeiptr mVertexBufferSize;
    GLsizeiptr mIndexBufferSize;
    bool mHasClipOrigin;
    bool mUseBufferSubData;
    std::shared_ptr<Shader> mShader;
};
