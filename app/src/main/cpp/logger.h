// Copyright (c) 2017-2020 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace Log {
enum class Level { Verbose, Debug, Info, Warning, Error };

void SetLevel(Level minSeverity);
void Write(Level severity, const std::string& msg);
void Write(Level severity, const char* fileName, const int line, const std::string& msg);
}  // namespace Log

std::string Fmt(const char* fmt, ...);
