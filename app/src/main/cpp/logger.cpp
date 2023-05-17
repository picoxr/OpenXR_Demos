// Copyright (c) 2017-2020 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "logger.h"

#include <sstream>

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR,   "demos", __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN,    "demos", __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO,    "demos", __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG,   "demos", __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "demos", __VA_ARGS__)

namespace {
    Log::Level g_minSeverity{Log::Level::Verbose};
    std::mutex g_logLock;
}  // namespace

namespace Log {
void SetLevel(Level minSeverity) { g_minSeverity = minSeverity; }

void Write(Level severity, const std::string& msg) {
    if (severity < g_minSeverity) {
        return;
    }
    const auto now = std::chrono::system_clock::now();
    const time_t now_time = std::chrono::system_clock::to_time_t(now);
    tm now_tm;
    localtime_r(&now_time, &now_tm);

    // time_t only has second precision. Use the rounding error to get sub-second precision.
    const auto secondRemainder = now - std::chrono::system_clock::from_time_t(now_time);
    const int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(secondRemainder).count();

    static std::map<Level, const char*> severityName = {
                            {Level::Verbose, "Verbose"},
                            {Level::Debug,   "Debug"},
                            {Level::Info,    "Info"},
                            {Level::Warning, "Warning"},
                            {Level::Error,   "Error"}};

    std::ostringstream out;
    out.fill('0');
    out << "[" << std::setw(2) << now_tm.tm_hour << ":" << std::setw(2) << now_tm.tm_min << ":" << std::setw(2) << now_tm.tm_sec
        << "." << std::setw(3) << milliseconds << "]"
        << "[" << severityName[severity] << "] " << msg << std::endl;

    std::lock_guard<std::mutex> lock(g_logLock);  // Ensure output is serialized
    ((severity == Level::Error) ? std::clog : std::cout) << out.str();

    if (severity == Level::Error) {
        ALOGE("%s", out.str().c_str());
    } else if (severity == Level::Warning) {
        ALOGW("%s", out.str().c_str());
    } else if (severity == Level::Info) {
        ALOGI("%s", out.str().c_str());
    } else if (severity == Level::Debug) {
        ALOGD("%s", out.str().c_str());
    } else {
        ALOGV("%s", out.str().c_str());
    }
}

void Write(Level severity, const char* fileName, const int line, const std::string& msg) {
    if (severity < g_minSeverity) {
        return;
    }
    const auto now = std::chrono::system_clock::now();
    const time_t now_time = std::chrono::system_clock::to_time_t(now);
    tm now_tm;
    localtime_r(&now_time, &now_tm);

    // time_t only has second precision. Use the rounding error to get sub-second precision.
    const auto secondRemainder = now - std::chrono::system_clock::from_time_t(now_time);
    const int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(secondRemainder).count();

    static std::map<Level, const char*> severityName = {
                            {Level::Verbose, "Verbose"},
                            {Level::Debug,   "Debug"},
                            {Level::Info,    "Info"},
                            {Level::Warning, "Warning"},
                            {Level::Error,   "Error"}};

    std::ostringstream out;
    out.fill('0');
    out << "[" << std::setw(2) << now_tm.tm_hour << ":" << std::setw(2) << now_tm.tm_min << ":" << std::setw(2) << now_tm.tm_sec
        << "." << std::setw(3) << milliseconds << "]"
        << "[" << severityName[severity] << "][" << fileName << ":" << line << "] " << msg << std::endl;

    std::lock_guard<std::mutex> lock(g_logLock);  // Ensure output is serialized
    ((severity == Level::Error) ? std::clog : std::cout) << out.str();

    if (severity == Level::Error) {
        ALOGE("%s", out.str().c_str());
    } else if (severity == Level::Warning) {
        ALOGW("%s", out.str().c_str());
    } else if (severity == Level::Info) {
        ALOGI("%s", out.str().c_str());
    } else if (severity == Level::Debug) {
        ALOGD("%s", out.str().c_str());
    } else {
        ALOGV("%s", out.str().c_str());
    }
}
}  // namespace Log

std::string Fmt(const char* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    int size = std::vsnprintf(nullptr, 0, fmt, vl);
    va_end(vl);

    if (size != -1) {
        std::unique_ptr<char[]> buffer(new char[size + 1]);

        va_start(vl, fmt);
        size = std::vsnprintf(buffer.get(), size + 1, fmt, vl);
        va_end(vl);
        if (size != -1) {
            return std::string(buffer.get(), size);
        }
    }

    throw std::runtime_error("Unexpected vsnprintf failure");
}