/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "text.h"
#include "utils.h"
#include <iostream>

Shader Text::mShader;
void Text::initShader() {
    static bool init = false;
    if (init) {
        return;
    } else {
        const char* vertexShaderCode = R"_(
            #version 320 es
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aTexCoords;
            out vec2 TexCoords;
            uniform mat4 projection;
            uniform mat4 view;
            uniform mat4 model;
            void main()
            {
                TexCoords = aTexCoords;
                gl_Position = projection * view * model * vec4(aPos, 1.0);
            }
        )_";

        const char* fragmentShaderCode = R"_(
            #version 320 es
            precision mediump float;
            in vec2 TexCoords;
            out vec4 FragColor;
            uniform vec3 textColor;
            uniform sampler2D texture;
            void main()
            {
                vec4 color = vec4(1.0, 1.0, 1.0, texture(texture, TexCoords).r);
                FragColor = vec4(textColor, 1.0) * color;
            }
        )_";
        mShader.loadShader(vertexShaderCode, fragmentShaderCode);
        init = true;
    }
}

Text::Text() {
}

Text::~Text() {
    mWordsMap.clear();
}

void Text::loadFaces(const wchar_t* text, int32_t length) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        errorf("initialize freetype error");
        return;
    }
    FT_Face face;
    std::string fft("font/Alibaba-PuHuiTi-Regular.ttf");

    std::vector<char> fftData = readFileFromAssets(fft.c_str());
    if (FT_New_Memory_Face(ft, (FT_Byte*)fftData.data(), fftData.size(), 0, &face)) {
        errorf("FT_New_Memory_Face error");
        return;
    }

    FT_Set_Pixel_Sizes(face, 96, 96);
    FT_Select_Charmap(face, ft_encoding_unicode);

    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    for (int32_t i = 0; i < length; ++i) {
        wchar_t ch = text[i];
        auto it = mWordsMap.find(ch);
        if (it != mWordsMap.end()) {
            continue;
        }

        if (FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT)) {
            errorf("TextRenderSample::LoadFacesByUnicode FREETYTPE: Failed to load Glyph");
            continue;
        }

        FT_Glyph glyph;
        FT_Get_Glyph(face->glyph, &glyph);

        //Convert the glyph to a bitmap.
        FT_Glyph_To_Bitmap(&glyph, ft_render_mode_normal, 0, 1);
        FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
        FT_Bitmap& bitmap = bitmap_glyph->bitmap;

        GLuint texture;
        GL_CALL(glGenTextures(1, &texture));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, bitmap.width, bitmap.rows, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bitmap.buffer));

        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

        Word word{texture, face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_left, face->glyph->bitmap_top, glyph->advance.x};
        mWordsMap[ch] = word;

        debugf("bitmap.width:%d, bitmap.rows:%d, bitmap_left:%d, bitmap_top:%d, advance.x:%d", 
            face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_left, face->glyph->bitmap_top, glyph->advance.x);
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

bool Text::initialize() {
    initShader();
    const wchar_t texts[] = L"-0123456789";
    loadFaces(texts, sizeof(texts) / sizeof(texts[0]) - 1);

    GL_CALL(glGenVertexArrays(1, &mVAO));
    GL_CALL(glGenBuffers(1, &mVBO));

    GL_CALL(glBindVertexArray(mVAO));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVBO));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 5, nullptr, GL_DYNAMIC_DRAW));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(float))));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, GL_NONE));
    GL_CALL(glBindVertexArray(GL_NONE));

    return true;
}

bool Text::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m, const wchar_t* text, int32_t length, const glm::vec3& color) {
    mShader.use();
    mShader.setUniformMat4("projection", p);
    mShader.setUniformMat4("view", v);
    mShader.setUniformMat4("model", m);
    mShader.setUniformVec3("textColor", color);

    GL_CALL(glBindVertexArray(mVAO));

    float scale = 0.001f;
    float xpos = 0.0f;
    float ypos = 0.0f;
    for (int32_t i = 0; i < length; ++i) {
        wchar_t ch = text[i];
        if (ch == L' ') {
            xpos += 60 * scale;
            continue;
        }
        auto it = mWordsMap.find(ch);
        if (it == mWordsMap.end()) {
            loadFaces(text + i, length - i);
            i--;
            continue;
        } else {
            Word word = it->second;

            GLfloat w = word.bitmap_width * scale;
            GLfloat h = word.bitmap_top * scale;

            xpos += word.bitmap_left * scale;

            GLfloat vertices[][5] = {
                    { xpos,     ypos + h, 0.0,   0.0, 0.0 },
                    { xpos,     ypos,     0.0,   0.0, 1.0 },
                    { xpos + w, ypos,     0.0,   1.0, 1.0 },

                    { xpos,     ypos + h, 0.0,   0.0, 0.0 },
                    { xpos + w, ypos,     0.0,   1.0, 1.0 },
                    { xpos + w, ypos + h, 0.0,   1.0, 0.0 }
            };

            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, word.textureId));
            //glUniform1i(m_SamplerLoc, 0);

            GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVBO));
            GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices));
            GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
            GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));

            xpos += w;
        }
    }
    GL_CALL(glBindVertexArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    return true;
}
