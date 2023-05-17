/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include "utils.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma) {
    std::string filename = std::string(path);
    if (directory != "") {
        filename = directory + '/' + filename;
    }
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) {
            format = GL_RED;
        } else if (nrComponents == 3) {
            format = GL_RGB;
        } else if (nrComponents == 4) {
            format = GL_RGBA;
        }
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        errorf("Texture failed to load at path: %s", path);
    }
    return textureID;
}

bool copyFile(const char* src, const char* dst) {
    FILE *fsrc = fopen(src, "rb");
    FILE *fdst = fopen(dst, "wb");
    if (fsrc == NULL || fdst == NULL) {
        if (fsrc) fclose(fsrc);
        if (fdst) fclose(fdst);
        return false;
    }
    char buffer[4096] = {0};
    size_t len = 0;
    while((len = fread(buffer, sizeof(char), sizeof(buffer), fsrc)) > 0) {
        if (fwrite(buffer, sizeof(char), len, fdst) < len) {
            if (fsrc) fclose(fsrc);
            if (fdst) fclose(fdst);
            return false;
        }
    }
    if (fsrc) fclose(fsrc);
    if (fdst) fclose(fdst);
    return true;
}

#include <jni.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
static AAssetManager *s_nativeasset = nullptr;
static JNIEnv *s_env = nullptr;
static jobject s_jobj;
static jmethodID s_mid;
//called by java
extern "C" JNIEXPORT 
void JNICALL Java_com_picovr_openxr_MainActivity_setNativeAssetManager(JNIEnv *env, jobject instance, jobject assetManager) {
    //s_env = env;
    s_nativeasset = AAssetManager_fromJava(env, assetManager);
    if (s_nativeasset == nullptr) {
        errorf("s_nativeasset is nullptr!");
    }

    jclass cls = env->GetObjectClass(instance);
    s_jobj = env->NewGlobalRef(instance);
    s_mid = env->GetMethodID(cls, "scanFile", "(Ljava/lang/String;)V");
    //env->CallVoidMethod(s_jobj, s_mid, env->NewStringUTF("/sdcard"));
}

void setJNIEnv(JNIEnv *env) {
    s_env = env;
}

unsigned int TextureFromFileAssets(const char* path, const std::string& directory, bool gamma) {
    std::string filename = std::string(path);
    if (directory != "") {
        filename = directory + '/' + filename;
    }
    unsigned int textureID;
    glGenTextures(1, &textureID);

    // read file from assets
    AAsset *pathAsset = AAssetManager_open(s_nativeasset, filename.c_str(), AASSET_MODE_UNKNOWN);
    off_t assetLength = AAsset_getLength(pathAsset);
    unsigned char *fileData = (unsigned char *) AAsset_getBuffer(pathAsset);

    int width, height, nrComponents;
    //unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    unsigned char *data = stbi_load_from_memory(fileData, assetLength, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) {
            format = GL_RED;
        } else if (nrComponents == 3) {
            format = GL_RGB;
        } else if (nrComponents == 4) {
            format = GL_RGBA;
        }
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        errorf("Texture failed to load at path: %s", path);
    }
    AAsset_close(pathAsset);
    return textureID;
}

std::vector<char> readFileFromAssets(const char* filename) {
    AAsset *pathAsset = AAssetManager_open(s_nativeasset, filename, AASSET_MODE_UNKNOWN);
    off_t assetLength = AAsset_getLength(pathAsset);
    unsigned char *fileData = (unsigned char *) AAsset_getBuffer(pathAsset);
    std::vector<char> buffer(assetLength);
    memcpy(buffer.data(), fileData, assetLength);
    AAsset_close(pathAsset);
    return buffer;
}

void refreshMedia(const std::string& path) {
    s_env->CallVoidMethod(s_jobj, s_mid, s_env->NewStringUTF(path.c_str()));
}
