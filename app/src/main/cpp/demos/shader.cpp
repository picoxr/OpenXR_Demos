/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include <iostream>
#include "shader.h"
#include "utils.h"

Shader::Shader() : mProgram(0) {
}

Shader::~Shader() {
    if (mProgram) {
        glDeleteProgram(mProgram);
    }
}

bool Shader::checkCompileErrors(GLuint shader, std::string type) {
    GLint success = 0;
    GLchar infoLog[1024] = { 0 };
    if (type != "PROGRAM") {
        GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &success));
        if (!success) {
            GL_CALL(glGetShaderInfoLog(shader, 1024, nullptr, infoLog));
            errorf("SHADER_COMPILATION_ERROR of type: %s %s", type.c_str(), infoLog);
            return false;
        }
    } else {
        GL_CALL(glGetProgramiv(shader, GL_LINK_STATUS, &success));
        if (!success) {
            GL_CALL(glGetProgramInfoLog(shader, 1024, nullptr, infoLog));
            errorf("PROGRAM_LINKING_ERROR of type: %s %s", type.c_str(), infoLog);
            return false;
        }
    }
    return true;
}

bool Shader::loadShader(const char* vertexShaderCode, const char* fragmentShaderCode) {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    GL_CALL(glShaderSource(vertex, 1, &vertexShaderCode, nullptr));
    GL_CALL(glCompileShader(vertex));
    if (!checkCompileErrors(vertex, "VERTEX")) {
        return false;
    }
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    GL_CALL(glShaderSource(fragment, 1, &fragmentShaderCode, nullptr));
    GL_CALL(glCompileShader(fragment));
    if (!checkCompileErrors(fragment, "FRAGMENT")) {
        return false;
    }
    mProgram = glCreateProgram();
    GL_CALL(glAttachShader(mProgram, vertex));
    GL_CALL(glAttachShader(mProgram, fragment));
    GL_CALL(glLinkProgram(mProgram));
    if (!checkCompileErrors(mProgram, "PROGRAM")) {
        return false;
    }
    GL_CALL(glDeleteShader(vertex));
    GL_CALL(glDeleteShader(fragment));
    return true;
}

void Shader::use() const {
    GL_CALL(glUseProgram(mProgram));
}

GLuint Shader::id() const {
    return mProgram;
}

void Shader::setUniformBool(const std::string& name, bool value) const {
    GL_CALL(glUniform1i(glGetUniformLocation(mProgram, name.c_str()), (int)value));
}

void Shader::setUniformInt(const std::string& name, int value) const {
    GL_CALL(glUniform1i(glGetUniformLocation(mProgram, name.c_str()), value));
}

void Shader::setUniformFloat(const std::string& name, float value) const {
    GL_CALL(glUniform1f(glGetUniformLocation(mProgram, name.c_str()), value));
}

void Shader::setUniformVec2(const std::string& name, const glm::vec2& value) const {
    GL_CALL(glUniform2fv(glGetUniformLocation(mProgram, name.c_str()), 1, &value[0]));
}

void Shader::setUniformVec2(const std::string& name, float x, float y) const {
    GL_CALL(glUniform2f(glGetUniformLocation(mProgram, name.c_str()), x, y));
}

void Shader::setUniformVec3(const std::string& name, const glm::vec3& value) const {
    GL_CALL(glUniform3fv(glGetUniformLocation(mProgram, name.c_str()), 1, &value[0]));
}

void Shader::setUniformVec3(const std::string& name, float x, float y, float z) const {
    GL_CALL(glUniform3f(glGetUniformLocation(mProgram, name.c_str()), x, y, z));
}

void Shader::setUniformVec4(const std::string& name, const glm::vec4& value) const {
    GL_CALL(glUniform4fv(glGetUniformLocation(mProgram, name.c_str()), 1, &value[0]));
}

void Shader::setUniformVec4(const std::string& name, float x, float y, float z, float w) const {
    GL_CALL(glUniform4f(glGetUniformLocation(mProgram, name.c_str()), x, y, z, w));
}

void Shader::setUniformMat2(const std::string& name, const glm::mat2& mat) const {
    GL_CALL(glUniformMatrix2fv(glGetUniformLocation(mProgram, name.c_str()), 1, GL_FALSE, &mat[0][0]));
}

void Shader::setUniformMat3(const std::string& name, const glm::mat3& mat) const {
    GL_CALL(glUniformMatrix3fv(glGetUniformLocation(mProgram, name.c_str()), 1, GL_FALSE, &mat[0][0]));
}

void Shader::setUniformMat4(const std::string& name, const glm::mat4& mat) const {
    GL_CALL(glUniformMatrix4fv(glGetUniformLocation(mProgram, name.c_str()), 1, GL_FALSE, &mat[0][0]));
}

GLuint Shader::getAttribLocation(const std::string& name) const {
    return glGetAttribLocation(mProgram, name.c_str());
}
