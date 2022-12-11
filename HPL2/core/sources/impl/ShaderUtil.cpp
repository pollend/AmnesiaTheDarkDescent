#include "graphics/ShaderUtil.h"

#include "bgfx/bgfx.h"
#include <bx/debug.h>
#include "graphics/BufferVertex.h"
#include "impl/LowLevelGraphicsSDL.h"
#include "impl/SDLTexture.h"
#include "system/LowLevelSystem.h"

#include "system/Platform.h"
#include "system/String.h"
#include <cstdint>
#include <cstring>

#ifdef WIN32
#include <io.h>
#endif

namespace hpl
{
    void HelperSubmitVertexBuffer(uint32_t stream, BufferVertexView vertexBuffer, BufferIndexView indexBuffer)
    {

        if (!vertexBuffer.IsEmpty())
        {
            auto& definition = vertexBuffer.GetVertexBuffer().GetDefinition();
            switch (definition.m_accessType)
            {
            case BufferVertex::AccessType::AccessStatic:
                bgfx::setVertexBuffer(stream, 
                    vertexBuffer.GetVertexBuffer().GetHandle(), vertexBuffer.Offset(), vertexBuffer.Count());
                break;
            case BufferVertex::AccessType::AccessDynamic:
            case BufferVertex::AccessType::AccessStream:
                bgfx::setVertexBuffer(stream, 
                    vertexBuffer.GetVertexBuffer().GetDynamicHandle(), vertexBuffer.Offset(), vertexBuffer.Count());
                break;
            }
        }

        if (!indexBuffer.IsEmpty())
        {
            auto& definition = indexBuffer.GetIndexBuffer().GetDefinition();
            switch (definition.m_accessType)
            {
            case BufferIndex::AccessType::AccessStatic:
                bgfx::setIndexBuffer(indexBuffer.GetIndexBuffer().GetHandle(), indexBuffer.Offset(), indexBuffer.Count());
                break;
            case BufferIndex::AccessType::AccessDynamic:
            case BufferIndex::AccessType::AccessStream:
                BX_ASSERT(false, "TODO: Implement dynamic index buffer");
                break;
            }
        }
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
        return bgfx::ProgramHandle(BGFX_INVALID_HANDLE);
    }
    bgfx::ProgramHandle loadProgram(bx::FileReaderI* _reader, const char* vsName, const char* fsName)
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

        return bgfx::ProgramHandle(BGFX_INVALID_HANDLE);
    }

    bgfx::ShaderHandle loadShader(bx::FileReaderI* _reader, const char* name)
    {
        return bgfx::ShaderHandle(BGFX_INVALID_HANDLE);
    }

    bgfx::ShaderHandle loadShader(const char* name)
    {
        return bgfx::ShaderHandle(BGFX_INVALID_HANDLE);
    }

} // namespace hpl