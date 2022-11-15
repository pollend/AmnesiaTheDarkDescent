#pragma once

#include <memory>
#include <bx/file.h>
#include <bgfx/bgfx.h>
#include "system/SystemTypes.h"

namespace hpl
{
    bgfx::ShaderHandle CreateShaderHandleFromFile(const tWString& asFile);
    bgfx::ShaderHandle CreateShaderHandleFromString(const char* apStringData);

    bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName);
    bgfx::ProgramHandle loadProgram(bx::FileReaderI* _reader, const char* vsName, const char* fsName);

    bgfx::ShaderHandle loadShader(bx::FileReaderI* _reader, const char* name);
    bgfx::ShaderHandle loadShader(const char* name);

}