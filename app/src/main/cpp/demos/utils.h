/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <string>
#include <vector>
#include "common/gfxwrapper_opengl.h"
#include "logger.h"

#define OPENGL_DEBUG
#ifdef OPENGL_DEBUG
#define GL_CALL(_CALL)  do { _CALL; GLenum gl_err = glGetError(); if (gl_err != 0) Log::Write(Log::Level::Error,__FILE__,__LINE__,Fmt("GL error %d returned from '%s'\n", gl_err,#_CALL)); } while (0)
#else
#define GL_CALL(_CALL)
#endif

#define errorf(...)   Log::Write(Log::Level::Error,   __FILE__, __LINE__, Fmt(__VA_ARGS__));
#define warnf(...)    Log::Write(Log::Level::Warning, __FILE__, __LINE__, Fmt(__VA_ARGS__));
#define infof(...)    Log::Write(Log::Level::Info,    __FILE__, __LINE__, Fmt(__VA_ARGS__));
#define debugf(...)   Log::Write(Log::Level::Debug,   __FILE__, __LINE__, Fmt(__VA_ARGS__));
#define verbosef(...) Log::Write(Log::Level::Verbose, __FILE__, __LINE__, Fmt(__VA_ARGS__));


bool copyFile(const char* src, const char* dst);
unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);
unsigned int TextureFromFileAssets(const char* path, const std::string& directory, bool gamma = false);
std::vector<char> readFileFromAssets(const char* file);
void refreshMedia(const std::string& path);
void setJNIEnv(JNIEnv *env);


#define HAND_LEFT  0
#define HAND_RIGHT 1
#define HAND_COUNT 2

#define EYE_LEFT  0
#define EYE_RIGHT 1
#define EYE_COUNT 2

#define GETSTR(str) #str

#define MEMBER_NAME(c, m) member_name<decltype(&c::m), &c::m>(#m)
template<typename T, T t>
constexpr const char *member_name(const char *name) noexcept {
    return name;
}
