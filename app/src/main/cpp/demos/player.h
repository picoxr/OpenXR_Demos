/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <thread>
#include <list>
#include <memory>
#include <vector>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <media/NdkMediaExtractor.h>
#include "shader.h"

typedef struct {
    float x;
    float y;
    float z;
}Position;
typedef struct {
    float x;
    float y;
}Coordinate;

typedef struct {
    Position position;
    Coordinate texCoords;
}SampleVertex2D;

typedef struct {
    Position position;
    Coordinate texCoords0;
    Coordinate texCoords1;
}SampleVertex3D;

typedef enum {
    mediaTypeVideo = 0,
    mediaTypeAudio
}mediaType;

typedef enum {
    playModel_None = 0,
    playModel_2D,
    playModel_2D_180,
    playModel_2D_360,
    playModel_3D_SBS,
    playModel_3D_SBS_360,
    playModel_3D_OU,
    playModel_3D_OU_360
}PlayModel;

typedef struct MediaFrame_tag {
    MediaFrame_tag() : type(mediaTypeVideo), pts(0), data(nullptr), size(0) {};
    mediaType type;
    uint64_t pts;
    int32_t width;
    int32_t height;
    uint8_t* data;
    uint32_t size;
    ssize_t bufferIndex;
    void* image;
}MediaFrame;

class Player {

public:
    Player();
    ~Player();

    bool initialize(EGLDisplay display);
    bool start(const std::string& file);
    bool stop();
    void setModel(const glm::mat4& m);
    bool render(const glm::mat4& p, const glm::mat4& v, int32_t eye);
    bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m, int32_t eye);
    void setPlayStyle(const PlayModel model);
    PlayModel getPlayStyle() const;

private:
    bool initShader();
    void InitializePfn();
    void threadDecode();
    void threadPlayAudio();

    std::shared_ptr<MediaFrame> getVideoFrame();
    std::shared_ptr<MediaFrame> getAudioFrame();
    bool releaseVideoFrame(std::shared_ptr<MediaFrame> &frame);
    bool releaseAudioFrame(std::shared_ptr<MediaFrame> &frame);

    void createVertexAndIndiceData(const PlayModel model);

private:
    friend void AImageReaderImageCallback(void* context, AImageReader* reader);

    static Shader mShader;
    GLuint mVAO;
    GLuint mVBO;
    GLuint mEBO;

    EGLDisplay mEglDisplay;
    PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC m_eglGetNativeClientBufferANDROID = nullptr;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_glEGLImageTargetTexture2DOES = nullptr;

    std::string      mFileName;
    AMediaExtractor* mExtractor;
    AMediaCodec*     mAudioCodec;
    int32_t          mFd;
    bool             mStarted;

    PlayModel        mPlayModel;
    uint64_t         mVideoPtsOffset;
    int64_t          mVideoDurationMs;

    int32_t          mAudioSampleRate;
    int32_t          mAudioChannelCount;

    size_t  mTrackCount;
    int32_t mVideoTrackIndex;
    int32_t mAudioTrackIndex;

    std::thread mThreadDecode;
    std::thread mThreadPlayAudio;
    bool mDecodeRunning;
    bool mPlayAudioRunning;

    std::list<std::shared_ptr<MediaFrame>> mDecodedVideoFrameList;
    std::list<std::shared_ptr<MediaFrame>> mDecodedAudioFrameList;
    std::mutex       mDecodedVideoFrameListMutex;
    std::mutex       mDecodedAudioFrameListMutex;

    glm::mat4 mModel;

    std::vector<SampleVertex2D> mVertexCoordinates2D;
    std::vector<SampleVertex3D> mVertexCoordinates3D;
    std::vector<GLuint>         mIndices;
};
