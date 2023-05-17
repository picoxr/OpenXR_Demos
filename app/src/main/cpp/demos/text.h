/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <vector>
#include <map>
#include "shader.h"
#include "ft2build.h"
#include "freetype/freetype.h"
#include "freetype/ftglyph.h"

typedef struct {
    GLuint textureId;
    uint32_t bitmap_width;
    uint32_t bitmap_rows;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    long advance;
}Word;

class Text {
public:
    Text();
    ~Text();
    bool initialize();
    bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m, const wchar_t* text, int32_t length, const glm::vec3& color);
private:
    void initShader();
    void loadFaces(const wchar_t* text, int32_t length);
private:
    static Shader mShader;
    std::map<int32_t, Word> mWordsMap;
    GLuint mVAO;
    GLuint mVBO;
};
