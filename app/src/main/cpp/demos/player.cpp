// Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved.
//
// This file provides an example of 3D decoding and play.
//
// Created by kevin-shunxiang at 2023/04/45
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <aaudio/AAudio.h>
#include <stddef.h>
#include "player.h"
#include "utils.h"

Shader Player::mShader;
Player::Player() : mExtractor(nullptr), mFd(-1), mStarted(false) {
    mVideoTrackIndex = -1;
    mAudioTrackIndex = -1;
    mDecodeRunning = mPlayAudioRunning = false;
    mVAO = mVBO= mEBO = 0;
    mPlayModel = playModel_None;
}

Player::~Player() {
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }
    if (mFd > 0) {
        close(mFd);
        mFd = -1;
    }
    if (mVAO != 0) {
        glDeleteVertexArrays(1, &mVAO);
    }
}

bool Player::initShader() {
    static bool init = false;
    if (init) {
        return true;
    }
    else {
        const GLchar* vertex_shader_glsl = R"_(
            #version 320 es
            precision highp float;
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec2 aTexCoord;
            uniform mat4 projection;
            uniform mat4 view;
            uniform mat4 model;
            out vec2 vTexCoord;
            void main()
            {
                vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
                gl_Position = projection * view * model * vec4(aPosition, 1.0);
            }
        )_";

        const GLchar* fragment_shader_glsl = R"_(
            #version 320 es
            #extension GL_OES_EGL_image_external_essl3 : require
            precision mediump float;
            in vec2 vTexCoord;
            uniform samplerExternalOES textureMap;
            out vec4 FragColor;
            void main()
            {
                FragColor = texture(textureMap, vTexCoord);
            }
        )_";

        if (mShader.loadShader(vertex_shader_glsl, fragment_shader_glsl) == false) {
            return false;
        }
        init = true;
    }
    return true;
}

void Player::InitializePfn() {
    m_eglGetNativeClientBufferANDROID = (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)eglGetProcAddress("eglGetNativeClientBufferANDROID");
    if (m_eglGetNativeClientBufferANDROID == nullptr) {
        errorf("eglGetNativeClientBufferANDROID is nullptr");
    }
    m_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if (m_eglCreateImageKHR == nullptr) {
        errorf("eglCreateImageKHR is nullptr");
    }
    m_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (m_eglDestroyImageKHR == nullptr) {
        errorf("eglDestroyImageKHR is nullptr");
    }
    m_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (m_glEGLImageTargetTexture2DOES == nullptr) {
        errorf("glEGLImageTargetTexture2DOES is nullptr ");
    }
}

bool Player::initialize(EGLDisplay display) {
    initShader();
    InitializePfn();
    mEglDisplay = display;

    GL_CALL(glGenVertexArrays(1, &mVAO));
    GL_CALL(glGenBuffers(1, &mVBO));
    GL_CALL(glGenBuffers(1, &mEBO));

    setPlayStyle(playModel_2D_360);

    return true;
}

void Player::setPlayStyle(PlayModel model) {
    if (model == mPlayModel) {
        return;
    }
    mPlayModel = model;
    createVertexAndIndiceData(mPlayModel);

    GLuint aPosition = mShader.getAttribLocation("aPosition");
    GLuint aTexCoord = mShader.getAttribLocation("aTexCoord");

    GL_CALL(glBindVertexArray(mVAO));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVBO));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO));

    GL_CALL(glEnableVertexAttribArray(aPosition));
    GL_CALL(glEnableVertexAttribArray(aTexCoord));

    if (mPlayModel == playModel_2D || mPlayModel == playModel_2D_180 || mPlayModel == playModel_2D_360) {
        GL_CALL(glBufferData(GL_ARRAY_BUFFER, mVertexCoordinates2D.size() * sizeof(SampleVertex2D), mVertexCoordinates2D.data(), GL_STATIC_DRAW));
        GL_CALL(glVertexAttribPointer(aPosition, sizeof(Position) / sizeof(float),   GL_FLOAT, GL_FALSE, sizeof(SampleVertex2D), (const void*)offsetof(SampleVertex2D, position)));
        GL_CALL(glVertexAttribPointer(aTexCoord, sizeof(Coordinate) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(SampleVertex2D), (const void*)offsetof(SampleVertex2D, texCoords)));
    } else {
        GL_CALL(glBufferData(GL_ARRAY_BUFFER, mVertexCoordinates3D.size() * sizeof(SampleVertex3D), mVertexCoordinates3D.data(), GL_STATIC_DRAW));
        GL_CALL(glVertexAttribPointer(aPosition, sizeof(Position) / sizeof(float),   GL_FLOAT, GL_FALSE, sizeof(SampleVertex3D), (const void*)offsetof(SampleVertex3D, position)));
    }
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(GLuint), mIndices.data(), GL_STATIC_DRAW));

    GL_CALL(glBindVertexArray(GL_NONE));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, GL_NONE));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE));
}

PlayModel Player::getPlayStyle() const {
    return mPlayModel;
}

#define PI 3.1415926535
#define RADIAN(x) ((x) * PI / 180)
void Player::createVertexAndIndiceData(const PlayModel model) {
    mIndices.resize(0);
    if (model == playModel_2D) {
        /* texture coordinate
         0(0,1)-------1(1,1)
            |            |
            |            |
         3(0,0)-------2(1,0)
        */
        mVertexCoordinates2D.resize(0);
        SampleVertex2D v0{{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}};  //left top
        SampleVertex2D v1{{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}};  //right top
        SampleVertex2D v2{{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}};  //right bottom
        SampleVertex2D v3{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}};  //left bottom
        mVertexCoordinates2D.push_back(v0);
        mVertexCoordinates2D.push_back(v1);
        mVertexCoordinates2D.push_back(v2);
        mVertexCoordinates2D.push_back(v3);

        mIndices.push_back(0); mIndices.push_back(3); mIndices.push_back(1);  //GL_CCW counterclockwise order 逆时针方向为正面
        mIndices.push_back(1); mIndices.push_back(3); mIndices.push_back(2);
    } else if (model == playModel_2D_360) {
        mVertexCoordinates2D.resize(0);
        int vertexCount = 0;
        float angleSpan = 2.0f;
        int32_t indexWidth = 0;
        float radius = 50.0f;
        for (float vAngle = 0; vAngle <= 180; vAngle += angleSpan) {      //vertical
            for (float hAngle = 0; hAngle <= 360; hAngle += angleSpan) {
                float x = (float) radius * sin(RADIAN(vAngle)) * sin(RADIAN(hAngle));
                float y = (float) radius * cos(RADIAN(vAngle));
                float z = (float) radius * sin(RADIAN(vAngle)) * cos(RADIAN(hAngle));

                float textureCoords_x = 1 - hAngle / 360;
                float textureCoords_y = 1 - vAngle / 180;

                SampleVertex2D v{{x, y, z}, {textureCoords_x, textureCoords_y}};
                mVertexCoordinates2D.push_back(v);
                
                if (vAngle == angleSpan && hAngle == 0) {
                    indexWidth = vertexCount;
                }
                if (vAngle > 0 && hAngle > 0) {
                    mIndices.push_back(vertexCount);
                    mIndices.push_back(vertexCount - indexWidth - 1);
                    mIndices.push_back(vertexCount - indexWidth);
                    mIndices.push_back(vertexCount);
                    mIndices.push_back(vertexCount - 1);
                    mIndices.push_back(vertexCount - indexWidth - 1);
                }
                vertexCount++;
            }
        }
    } else if (model == playModel_3D_SBS) {
        mVertexCoordinates3D.resize(0);
        SampleVertex3D v0{{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {0.5f, 1.0f}};  //left top
        SampleVertex3D v1{{ 0.5f,  0.5f, 0.0f}, {0.5f, 1.0f}, {1.0f, 1.0f}};  //right top
        SampleVertex3D v2{{ 0.5f, -0.5f, 0.0f}, {0.5f, 0.0f}, {1.0f, 0.0f}};  //right bottom
        SampleVertex3D v3{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {0.5f, 0.0f}};  //left bottom
        mVertexCoordinates3D.push_back(v0);
        mVertexCoordinates3D.push_back(v1);
        mVertexCoordinates3D.push_back(v2);
        mVertexCoordinates3D.push_back(v3);

        mIndices.push_back(0); mIndices.push_back(3); mIndices.push_back(1);  //GL_CCW counterclockwise order 逆时针方向为正面
        mIndices.push_back(1); mIndices.push_back(3); mIndices.push_back(2);
    } else if (model == playModel_3D_OU) {
        mVertexCoordinates3D.resize(0);
        SampleVertex3D v0{{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.5f}};  //left top
        SampleVertex3D v1{{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.5f}};  //right top
        SampleVertex3D v2{{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.5f}, {1.0f, 0.0f}};  //right bottom
        SampleVertex3D v3{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.5f}, {0.0f, 0.0f}};  //left bottom
        mVertexCoordinates3D.push_back(v0);
        mVertexCoordinates3D.push_back(v1);
        mVertexCoordinates3D.push_back(v2);
        mVertexCoordinates3D.push_back(v3);

        mIndices.push_back(0); mIndices.push_back(3); mIndices.push_back(1);  //GL_CCW counterclockwise order 逆时针方向为正面
        mIndices.push_back(1); mIndices.push_back(3); mIndices.push_back(2);
    }
}

bool Player::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m, int32_t eye) {

    std::shared_ptr<MediaFrame> frame = getVideoFrame();
    if (frame.get() == nullptr) {
        return false;
    }

    AImage* image = reinterpret_cast<AImage*>(frame->image);
    AHardwareBuffer* hwBuff = nullptr;
    if (AImage_getHardwareBuffer(image, &hwBuff) != AMEDIA_OK) {
        errorf("AImage_getHardwareBuffer error ");
        return false;
    }
    EGLClientBuffer clientBuffer = m_eglGetNativeClientBufferANDROID(hwBuff);
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLImageKHR imagekhr = m_eglCreateImageKHR(mEglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, eglImageAttributes);
    if (imagekhr == nullptr) {
        errorf("imagekhr is nullptr ");
        return false;
    }

    mShader.use(); 
    mShader.setUniformMat4("projection", p);
    mShader.setUniformMat4("view", v);
    mShader.setUniformMat4("model", m);

    GL_CALL(glFrontFace(GL_CCW));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_CULL_FACE));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glBindVertexArray(mVAO));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVBO));

    if (mPlayModel >= playModel_3D_SBS) {
        GLuint aTexCoord = mShader.getAttribLocation("aTexCoord");
        GL_CALL(glEnableVertexAttribArray(aTexCoord));
        if (eye == EYE_LEFT) {
            GL_CALL(glVertexAttribPointer(aTexCoord, sizeof(Coordinate) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(SampleVertex3D), (const void*)offsetof(SampleVertex3D, texCoords0)));
        } else {
            GL_CALL(glVertexAttribPointer(aTexCoord, sizeof(Coordinate) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(SampleVertex3D), (const void*)offsetof(SampleVertex3D, texCoords1)));
        }
    }

    m_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, imagekhr);

    GL_CALL(glDrawElements(GL_TRIANGLES, mIndices.size(), GL_UNSIGNED_INT, (const void*)0));

    m_eglDestroyImageKHR(mEglDisplay, imagekhr);
    releaseVideoFrame(frame);

    return true;
}

bool Player::start(const std::string& file) {
    if (mFileName == file) {
        return true;
    }
    stop();

    mFileName = file;
    struct stat64 statbuff;
    int64_t fileLen = -1;
    if (stat64(file.c_str(), &statbuff) < 0) {
        errorf("setDataSource error, open file %s error", file.c_str());
        return false;  
    } else {  
        fileLen = statbuff.st_size;  
    }
    mFd = open(file.c_str(), O_RDONLY);
    if (mFd < 0) {
        errorf("setDataSource error, open file %s error(%d), ret=%d", file.c_str(), errno, mFd);
        return false;
    }

    /*media_status_t status = AMediaExtractor_setDataSource(mExtractor, file.c_str());
    if (status != AMEDIA_OK) {
        errorf("setDataSource error, ret = %d", status);
        return false;
    }*/
    if (mExtractor == nullptr) {
        mExtractor = AMediaExtractor_new();
    }
    media_status_t status = AMediaExtractor_setDataSourceFd(mExtractor, mFd, 0, fileLen);
    if (status != AMEDIA_OK) {
        errorf("setDataSource error, ret = %d", status);
        return false;
    }
    mTrackCount = AMediaExtractor_getTrackCount(mExtractor);
    if (mTrackCount > 0) {
        mThreadDecode = std::thread(&Player::threadDecode, this);
    } else {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
        return false;
    }
    infof("video file %s size %lld track = %d", file.c_str(), fileLen, mTrackCount);
    return true;
}

bool Player::stop() {
    infof("stop+++");
    mDecodeRunning = false;
    mPlayAudioRunning = false;
    if (mThreadDecode.joinable()) {
        infof("mThreadDecode.join()+++");
        mThreadDecode.join();
        infof("mThreadDecode.join()---");
    }
    if (mThreadPlayAudio.joinable()) {
        infof("mThreadPlayAudio.join()+++");
        mThreadPlayAudio.join();
        infof("mThreadPlayAudio.join()---");
    }
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }

    mDecodedVideoFrameListMutex.lock();
    mDecodedVideoFrameList.clear();
    mDecodedVideoFrameListMutex.unlock();

    mDecodedAudioFrameListMutex.lock();
    mDecodedAudioFrameList.clear();
    mDecodedAudioFrameListMutex.unlock();

    if (mFd > 0) {
        close(mFd);
        mFd = -1;
    }
    infof("stop---");
    return true;
}

bool Player::render(const glm::mat4& p, const glm::mat4& v, int32_t eye) {
    return render(p, v, mModel, eye);
}

void AImageReaderImageCallback(void* context, AImageReader* reader) {
    Player* thiz = (Player*)context;
    AImage* image = nullptr;
    if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK) {
        errorf("AImageReader_acquireLatestImage");
        return;
    }
    std::int64_t presentationTimeMs = 0;
    AImage_getTimestamp(image, &presentationTimeMs);
    presentationTimeMs = static_cast<std::uint64_t>(presentationTimeMs * 0.000001);
    int32_t width = 0, height = 0;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);

    std::shared_ptr<MediaFrame> frame = std::make_shared<MediaFrame>();
    frame->type = mediaTypeVideo;
    frame->width = width;
    frame->height = height;
    frame->pts = (presentationTimeMs + thiz->mVideoPtsOffset);
    frame->data = nullptr;
    frame->size = 0;
    frame->bufferIndex = -1;
    frame->image = (void*)image;

    thiz->mDecodedVideoFrameListMutex.lock();
    thiz->mDecodedVideoFrameList.push_back(frame);
    //infof("mDecodedVideoFrameList size:%d", thiz->mDecodedVideoFrameList.size());
    thiz->mDecodedVideoFrameListMutex.unlock();
}

void Player::threadDecode() {
    infof("threadDecode+++");
    //create surface
    ANativeWindow* surface = nullptr;
    AImageReader* imageReader = nullptr;
    const int32_t maxImageCount = 12;
    const uint64_t imageReaderFlags = AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER | AHARDWAREBUFFER_USAGE_CPU_READ_NEVER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    AImageReader_ImageListener imageListener;
    imageListener.context = this;
    imageListener.onImageAvailable = &AImageReaderImageCallback;

    if (AImageReader_newWithUsage(1, 1, AIMAGE_FORMAT_PRIVATE, imageReaderFlags, maxImageCount, &imageReader) != AMEDIA_OK) {
        errorf("AImageReader_newWithUsage error");
        return;
    }
    if (AImageReader_setImageListener(imageReader, &imageListener) != AMEDIA_OK) {
        errorf("AImageReader_setImageListener error");
        return;
    }
    if (AImageReader_getWindow(imageReader, &surface) != AMEDIA_OK) {
        errorf("AImageReader_getWindow error");
        return;
    }

    AMediaCodec* videoCodec = nullptr;
    AMediaCodec* audioCodec = nullptr;

    for (auto i = 0; i < mTrackCount; i++) {
        const char *mime = nullptr;
        AMediaFormat *format = AMediaExtractor_getTrackFormat(mExtractor, i);
        infof("track %d format: %s", i, AMediaFormat_toString(format));
        AMediaFormat_getString(format, "mime", &mime);
        if (strstr(mime, "video")) {
            mVideoTrackIndex = i;
            int64_t videoDurationUs = 0;
            AMediaFormat_getInt64(format, "durationUs", &videoDurationUs);
            mVideoDurationMs = videoDurationUs / 1000;
            videoCodec = AMediaCodec_createDecoderByType(mime);
            if (videoCodec == nullptr) {
                errorf("create mediacodec %s error", mime);
                return;
            }
            if (AMediaCodec_configure(videoCodec, format, surface, nullptr, 0) != AMEDIA_OK) {
                errorf("AMediaCodec_configure error");
                return;
            }
            if (AMediaCodec_start(videoCodec) != AMEDIA_OK) {
                errorf("AMediaCodec_start error");
                return;
            }
        } else if (strstr(mime, "audio")) {
            mAudioTrackIndex = i;
            AMediaFormat_getInt32(format, "channel-count", &mAudioChannelCount);
            AMediaFormat_getInt32(format, "sample-rate", &mAudioSampleRate);
            audioCodec = AMediaCodec_createDecoderByType(mime);
            if (audioCodec == nullptr) {
                errorf("create mediacodec %s error", mime);
                return;
            }
            if (AMediaCodec_configure(audioCodec, format, nullptr, nullptr, 0) != AMEDIA_OK) {
                errorf("AMediaCodec_configure error");
                return;
            }
            if (AMediaCodec_start(audioCodec) != AMEDIA_OK) {
                errorf("AMediaCodec_start error");
                return;
            }
            mAudioCodec = audioCodec;
            mThreadPlayAudio = std::thread(&Player::threadPlayAudio, this);
        }
        AMediaExtractor_selectTrack(mExtractor, i);
        AMediaFormat_delete(format);
    }

    mVideoPtsOffset = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    mDecodeRunning = true;
    while (mDecodeRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        mDecodedVideoFrameListMutex.lock();
        int32_t videoFrameListSize = mDecodedVideoFrameList.size();
        mDecodedVideoFrameListMutex.unlock();
        if (videoFrameListSize > 5) {
            continue;
        }

        int32_t index = AMediaExtractor_getSampleTrackIndex(mExtractor);
        int64_t pts = AMediaExtractor_getSampleTime(mExtractor);
        AMediaCodec* codec = nullptr;
        //infof("get index:%d, pts:%lld", index, pts);
        if (index < 0) {
            // Play from the beginning when reach end of the file
            infof("the video file is end");
            AMediaExtractor_seekTo(mExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
            mVideoPtsOffset += mVideoDurationMs;
            continue;
        } else if (index == mAudioTrackIndex) {
            codec = audioCodec;
        } else if (index == mVideoTrackIndex) {
            codec = videoCodec;
        }

        // input
        ssize_t bufferIndex = AMediaCodec_dequeueInputBuffer(codec, 1);
        //infof("AMediaCodec_dequeueInputBuffer bufferIndex:%d", bufferIndex);
        if (bufferIndex >= 0) {
            size_t bufferSize = 0;
            uint8_t *buffer = AMediaCodec_getInputBuffer(codec, bufferIndex, &bufferSize);
            ssize_t size = AMediaExtractor_readSampleData(mExtractor, buffer, bufferSize);
            AMediaCodec_queueInputBuffer(codec, bufferIndex, 0, size, pts, 0);
        }
        AMediaExtractor_advance(mExtractor);

        //output
        AMediaCodecBufferInfo outputBufferInfo{};
        bufferIndex = AMediaCodec_dequeueOutputBuffer(codec, &outputBufferInfo, 10);
        if (bufferIndex >= 0) {
            if (outputBufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                Log::Write(Log::Level::Error, Fmt("video codec end"));
            }
            if (codec == videoCodec) {
                if (bufferIndex != AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED && bufferIndex != AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED && bufferIndex != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                    //infof("to AMediaCodec_releaseOutputBuffer");
                    AMediaCodec_releaseOutputBuffer(codec, bufferIndex, true);
                }
            } else {
                uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, bufferIndex, nullptr);
                if (outputBuffer) {
                    std::shared_ptr<MediaFrame> frame = std::make_shared<MediaFrame>();
                    frame->type = mediaTypeAudio;
                    frame->pts = outputBufferInfo.presentationTimeUs / 1000;
                    frame->data = outputBuffer + outputBufferInfo.offset;
                    frame->size = outputBufferInfo.size;
                    frame->bufferIndex = bufferIndex;

                    mDecodedAudioFrameListMutex.lock();
                    mDecodedAudioFrameList.push_back(frame);
                    //infof("mDecodedAudioFrameList size:%d", mDecodedAudioFrameList.size());
                    mDecodedAudioFrameListMutex.unlock();
                }
            }
        }
    }
    infof("threadDecode---");
}


void Player::threadPlayAudio() {
    infof("threadPlayAudio+++");
    AAudioStreamBuilder *builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != 0) {
        errorf("AAudio_createStreamBuilder result=%d", result);
    }
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setSampleRate(builder, mAudioSampleRate);
    AAudioStreamBuilder_setChannelCount(builder, mAudioChannelCount);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, 2);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    AAudioStream *stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    if (result != 0) {
        errorf("AAudioStreamBuilder_openStream result=%d", result);
    }
    result = AAudioStream_requestStart(stream);
    if (result != 0) {
        errorf("AAudioStream_requestStart result:%d %s", result, AAudio_convertResultToText(result));
    }

    mPlayAudioRunning = true;
    while (mPlayAudioRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::shared_ptr<MediaFrame> frame = getAudioFrame();
        if (frame.get()) {
            int32_t numFrames = frame->size / (mAudioChannelCount * sizeof(int16_t));
            int64_t timeout = 1000000 * numFrames / mAudioSampleRate;
            result = AAudioStream_write(stream, frame->data, numFrames, timeout);
            if (result < 0) {
                errorf("AAudioStream_write result:%d %s", result, AAudio_convertResultToText(result));
            } else if (result < numFrames) {
                errorf("AAudioStream_write result:%d < numFrames %d", result, numFrames);
                releaseAudioFrame(frame);
            } else {
                releaseAudioFrame(frame);
            }
        }
    }
    AAudioStream_close(stream);
    AAudioStreamBuilder_delete(builder);
    infof("threadPlayAudio---");
}

std::shared_ptr<MediaFrame> Player::getVideoFrame() {
    std::lock_guard<std::mutex> guard(mDecodedVideoFrameListMutex);
    if (mDecodedVideoFrameList.size()) {
        return mDecodedVideoFrameList.front();
    } else {
        return nullptr;
    }
}
std::shared_ptr<MediaFrame> Player::getAudioFrame() {
    std::lock_guard<std::mutex> guard(mDecodedAudioFrameListMutex);
    if (mDecodedAudioFrameList.size()) {
        return mDecodedAudioFrameList.front();
    } else {
        return nullptr;
    }
}

bool Player::releaseVideoFrame(std::shared_ptr<MediaFrame> &frame) {
    if (frame.get() == nullptr) {
        return true;
    }
    std::lock_guard<std::mutex> guard(mDecodedVideoFrameListMutex);
    if (mDecodedVideoFrameList.size() <= 1) {
        return true;
    }
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();  //in millisecond
    if (now < frame->pts) {
        //infof("releaseFrame now:%lld < frame.pts:%lld", now, frame->pts);
        return false;
    }
    auto &it = mDecodedVideoFrameList.front();
    if (frame->image) {
        AImage_delete((AImage*)frame->image);
    }
    mDecodedVideoFrameList.pop_front();
    //infof("pop mDecodedVideoFrameList size:%d", mDecodedVideoFrameList.size());
    return true;
}
bool Player::releaseAudioFrame(std::shared_ptr<MediaFrame> &frame) {
    std::lock_guard<std::mutex> guard(mDecodedAudioFrameListMutex);
    if (mDecodedAudioFrameList.size()) {
        auto &it = mDecodedAudioFrameList.front();
        AMediaCodec_releaseOutputBuffer(mAudioCodec, it->bufferIndex, true);
        mDecodedAudioFrameList.pop_front();
    }
    //infof("pop mDecodedAudioFrameList size:%d", mDecodedAudioFrameList.size());
    return true;
}

void Player::setModel(const glm::mat4& m) {
    mModel = m;
}
