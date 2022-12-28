#include "graphics/ShaderUtil.h"

#include "bgfx/bgfx.h"
#include <bx/debug.h>
#include "impl/LowLevelGraphicsSDL.h"
#include "impl/SDLTexture.h"
#include "system/LowLevelSystem.h"

#include "system/Platform.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <bx/debug.h>
#include <system/bootstrap.h>

#ifdef WIN32
#include <io.h>
#endif

namespace hpl
{
    static const bgfx::Memory* loadMem(bx::FileReaderI* _reader, const char* _filePath)
    {
        if (bx::open(_reader, _filePath) )
        {
            uint32_t size = (uint32_t)bx::getSize(_reader);
            const bgfx::Memory* mem = bgfx::alloc(size+1);
            bx::read(_reader, mem->data, size, bx::ErrorAssert{});
            bx::close(_reader);
            mem->data[mem->size-1] = '\0';
            return mem;
        }

        bx::debugPrintf("Failed to load %s.", _filePath);
        return nullptr;
    }

    bgfx::ShaderHandle CreateShaderHandleFromFile(const tWString& asFile)
    {
        ///////////////////////////////////////
        // Load file
        FILE* pFile = cPlatform::OpenFile(asFile, _W("rb"));
        if (pFile == NULL)
        {
            Error("Could not open file %ls\n", asFile.c_str());
            return bgfx::ShaderHandle{ 0 };
        }

        fseek(pFile, 0, SEEK_END);
        int lFileSize = ftell(pFile);
        rewind(pFile);

        char* pBuffer = (char*)hplMalloc(sizeof(char) * lFileSize + 1);

        fread(pBuffer, sizeof(char), lFileSize, pFile);
        pBuffer[lFileSize] = 0; // Zero at end so it is a proper string.
        fclose(pFile);

        bgfx::ShaderHandle handle = CreateShaderHandleFromString(pBuffer);
        hplFree(pBuffer);
        return handle;
    }

    bgfx::ShaderHandle CreateShaderHandleFromString(const char* strData)
    {
        bgfx::Memory memory = { 0 };
        memory.data = (uint8_t*)strData;
        memory.size = strlen(strData) + 1;
        return bgfx::createShader(&memory);
    }

    bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName)
    {
        return loadProgram(hpl::bootstrap::GetFileReader(), vsName, fsName);
    }
    
    bgfx::ProgramHandle loadProgram(bx::FileReaderI* reader, const char* vsName, const char* fsName)
    {
        return bgfx::createProgram(loadShader(reader, vsName), loadShader(reader, fsName), true);   
    }

    bgfx::ShaderHandle loadShader(bx::FileReaderI* reader, const char* name)
    {
        const char* shaderPath = "";
        switch (bgfx::getRendererType())
        {
        case bgfx::RendererType::Noop:
        case bgfx::RendererType::Direct3D9:
            shaderPath = "shaders/dx9/";
            break;
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12:
            shaderPath = "shaders/dx11/";
            break;
        case bgfx::RendererType::Agc:
        case bgfx::RendererType::Gnm:
            shaderPath = "shaders/pssl/";
            break;
        case bgfx::RendererType::Metal:
            shaderPath = "shaders/metal/";
            break;
        case bgfx::RendererType::Nvn:
            shaderPath = "shaders/nvn/";
            break;
        case bgfx::RendererType::OpenGL:
            shaderPath = "shaders/glsl/";
            break;
        case bgfx::RendererType::OpenGLES:
            shaderPath = "shaders/essl/";
            break;
        case bgfx::RendererType::Vulkan:
            shaderPath = "shaders/spirv/";
            break;
        case bgfx::RendererType::WebGPU:
            shaderPath = "shaders/spirv/";
            break;
        case bgfx::RendererType::Count:
            BX_ASSERT(false, "You should not be here!");
            break;
        }
        char filePath[1024] = { '\0'};
        bx::strCopy(filePath, BX_COUNTOF(filePath), shaderPath);
        bx::strCat(filePath, BX_COUNTOF(filePath), name);
        bx::strCat(filePath, BX_COUNTOF(filePath), ".bin");
        
        bgfx::ShaderHandle handle = bgfx::createShader(loadMem(reader, filePath) );
	    bgfx::setName(handle, name);

        return handle;
    }

    bgfx::ShaderHandle loadShader(const char* name)
    {
        return loadShader(hpl::bootstrap::GetFileReader(), name);
    }

} // namespace hpl