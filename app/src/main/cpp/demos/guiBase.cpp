/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "guiBase.h"
#include "utils.h"

GuiBase& GuiBase::instance() {
    static GuiBase guiBase;
    return guiBase;
}

GuiBase::GuiBase() : mImguiContext(nullptr), mShaderHandle(0), mUseBufferSubData(true), mFontTexture(0) {
    mImguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(mImguiContext);
}

GuiBase::~GuiBase() {
    if (mImguiContext) {
        ImGui::DestroyContext(mImguiContext);
    }
}

void GuiBase::initShader() {
    if (mShader.get()) {
        return;
    }
    mShader = std::make_shared<Shader>();
    const GLchar* vertex_shader_glsl = R"_(
        #version 320 es
        precision highp float;
        layout (location = 0) in vec2 Position;
        layout (location = 1) in vec2 UV;
        layout (location = 2) in vec4 Color;
        uniform mat4 ProjMtx;
        out vec2 Frag_UV;
        out vec4 Frag_Color;
        void main()
        {
            Frag_UV = UV;
            Frag_Color = Color;
            gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1);
        }
    )_";

    const GLchar* fragment_shader_glsl = R"_(
        #version 320 es
        precision mediump float;
        uniform sampler2D Texture;
        in vec2 Frag_UV;
        in vec4 Frag_Color;
        layout (location = 0) out vec4 Out_Color;
        void main()
        {
            Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
        }
    )_";

    mShader->loadShader(vertex_shader_glsl, fragment_shader_glsl);
    mShaderHandle = mShader->id();
    mAttribLocationTex = glGetUniformLocation(mShaderHandle, "Texture");
    mAttribLocationProjMtx = glGetUniformLocation(mShaderHandle, "ProjMtx");
    mAttribLocationVtxPos = (GLuint)glGetAttribLocation(mShaderHandle, "Position");
    mAttribLocationVtxUV = (GLuint)glGetAttribLocation(mShaderHandle, "UV");
    mAttribLocationVtxColor = (GLuint)glGetAttribLocation(mShaderHandle, "Color");
}

bool GuiBase::initialize() {
    initShader();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags = ImGuiBackendFlags_HasSetMousePos;

    if (mFontTexture == 0) {
        unsigned char* pixels;
        int32_t width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        GL_CALL(glGenBuffers(1, &mVboHandle));
        GL_CALL(glGenBuffers(1, &mElementsHandle));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
        GL_CALL(glGenTextures(1, &mFontTexture));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, mFontTexture));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    } else {
        unsigned char* pixels;
        io.Fonts->GetTexDataAsRGBA32(&pixels, nullptr, nullptr);
    }
    io.Fonts->SetTexID((ImTextureID)(intptr_t)mFontTexture);
    return true;
}

#define IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
#define GL_CLIP_ORIGIN

void GuiBase::setupRenderState(ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object) {
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendEquation(GL_FUNC_ADD));
    GL_CALL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glDisable(GL_CULL_FACE));
    GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glDisable(GL_STENCIL_TEST));
    GL_CALL(glEnable(GL_SCISSOR_TEST));

    //GL_CALL(glDisable(GL_PRIMITIVE_RESTART));

    // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
#if defined(GL_CLIP_ORIGIN)
    bool clip_origin_lower_left = true;
#endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    GL_CALL(glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height));
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
#if defined(GL_CLIP_ORIGIN)
    if (!clip_origin_lower_left) { float tmp = T; T = B; B = tmp; } // Swap top and bottom if origin is upper left
#endif
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };

    GL_CALL(glUseProgram(mShaderHandle));
    GL_CALL(glUniform1i(mAttribLocationTex, 0));
    GL_CALL(glUniformMatrix4fv(mAttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0])); 

    glBindSampler(0, 0);
    glBindVertexArray(vertex_array_object);

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVboHandle));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mElementsHandle));
    GL_CALL(glEnableVertexAttribArray(mAttribLocationVtxPos));
    GL_CALL(glEnableVertexAttribArray(mAttribLocationVtxUV));
    GL_CALL(glEnableVertexAttribArray(mAttribLocationVtxColor));
    GL_CALL(glVertexAttribPointer(mAttribLocationVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos)));
    GL_CALL(glVertexAttribPointer(mAttribLocationVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv)));
    GL_CALL(glVertexAttribPointer(mAttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col)));
}

bool GuiBase::renderDrawData(ImDrawData* draw_data) {
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0) {
        return false;
    }
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);


    // Backup GL state
    GLenum last_active_texture = 0; GL_CALL(glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture));
    glActiveTexture(GL_TEXTURE0);
    GLuint last_program = 0; glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&last_program);
    GLuint last_texture = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&last_texture);
    GLuint last_sampler = 0; glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&last_sampler);
    GLuint last_array_buffer = 0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*)&last_array_buffer);
    GLuint last_vertex_array_object = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint*)&last_vertex_array_object);
    //GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_stencil_test = glIsEnabled(GL_STENCIL_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    //GLboolean last_enable_primitive_restart = glIsEnabled(GL_PRIMITIVE_RESTART);


    GLuint vertex_array_object = 0;
    GL_CALL(glGenVertexArrays(1, &vertex_array_object));

    setupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        // Upload vertex/index buffers
        // - OpenGL drivers are in a very sorry state nowadays....
        //   During 2021 we attempted to switch from glBufferData() to orphaning+glBufferSubData() following reports
        //   of leaks on Intel GPU when using multi-viewports on Windows.
        // - After this we kept hearing of various display corruptions issues. We started disabling on non-Intel GPU, but issues still got reported on Intel.
        // - We are now back to using exclusively glBufferData(). So bd->UseBufferSubData IS ALWAYS FALSE in this code.
        //   We are keeping the old code path for a while in case people finding new issues may want to test the bd->UseBufferSubData path.
        // - See https://github.com/ocornut/imgui/issues/4468 and please report any corruption issues.
        const GLsizeiptr vtx_buffer_size = (GLsizeiptr)cmd_list->VtxBuffer.Size * (int)sizeof(ImDrawVert);
        const GLsizeiptr idx_buffer_size = (GLsizeiptr)cmd_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx);
        if (mUseBufferSubData) {
            if (mVertexBufferSize < vtx_buffer_size) {
                mVertexBufferSize = vtx_buffer_size;
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, mVertexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            if (mIndexBufferSize < idx_buffer_size) {
                mIndexBufferSize = idx_buffer_size;
                GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, 0, vtx_buffer_size, (const GLvoid*)cmd_list->VtxBuffer.Data));
            GL_CALL(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, idx_buffer_size, (const GLvoid*)cmd_list->IdxBuffer.Data));
        } else {
            GL_CALL(glBufferData(GL_ARRAY_BUFFER, vtx_buffer_size, (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW));
            GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_buffer_size, (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW));
        }

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState) {
                    setupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
                }
                else {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }
                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                GL_CALL(glScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y)));
                // Bind texture, Draw
                GL_CALL(glBindTexture(GL_TEXTURE_2D, mFontTexture));
                GL_CALL(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx))));
            }
        }
    }

    // Destroy the temporary VAO
    GL_CALL(glDeleteVertexArrays(1, &vertex_array_object));
    if (last_program == 0 || glIsProgram(last_program)) glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array_object);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_stencil_test) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    //if (last_enable_primitive_restart) glEnable(GL_PRIMITIVE_RESTART); else glDisable(GL_PRIMITIVE_RESTART);
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);

    return true;
}

void GuiBase::render() {
    GuiBase::renderDrawData(ImGui::GetDrawData());
}

