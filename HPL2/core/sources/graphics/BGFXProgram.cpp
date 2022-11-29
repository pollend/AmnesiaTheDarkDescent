#include "bgfx/bgfx.h"
#include <algorithm>
#include <graphics/BGFXProgram.h>

namespace hpl
{
    // BGFXProgram::BGFXProgram(const tString& asName,eGpuProgramFormat aProgramFormat) : iGpuProgram(asName, aProgramFormat) {

    // }

    // BGFXProgram::~BGFXProgram() {

    // }

    // bool BGFXProgram::Link()
    // {
    //     BX_ASSERT(false, "Not implemented need to submit");
    //     return true;
    // }

    // void BGFXProgram::Bind()
    // {
    //     BX_ASSERT(false, "Not implemented need to submit");
    // }

    // void BGFXProgram::UnBind()
    // {
    //     BX_ASSERT(false, "Not implemented need to submit");
    // }

    // BGFXProgram& BGFXProgram::SetSubmitHandler(BGFXProgram::SubmitHandler handler)
    // {
    //     _submitHandler = handler;
    //     return *this;
    // }

    // BGFXProgram& BGFXProgram::SetUniformHandler(BGFXProgram::UniformDataHandler handler)
    // {
    //     _uniformHandler = handler;
    //     return *this;
    // }

    // BGFXProgram& BGFXProgram::SetVariableHandler(VariableDataHandler handler)
    // {
    //     _variableHandler = handler;
    //     return *this;
    // }

    // void BGFXProgram::Submit(bgfx::ViewId view, GraphicsContext& context)
    // {
    //     if (_submitHandler)
    //     {
    //         _submitHandler(view, context);
    //     }
    //     bgfx::submit(view, _handler);
    // };

    // bool BGFXProgram::SetUniform(int varId, const BGFXProgram::UniformData& data)
    // {
    //     if (_uniformHandler)
    //     {
    //         return _uniformHandler(varId, data);
    //     }
    //     return false;
    // }

    // bool BGFXProgram::SetInt(int alVarId, int alX)
    // {
    //     UniformData data;
    //     data.input.i = alX;
    //     data.type = UniformType_Int;
    //     return SetUniform(alVarId, data);
    // }

    // bool BGFXProgram::SetFloat(int alVarId, float afX)
    // {
    //     UniformData data;
    //     data.input.f = afX;
    //     data.type = UniformType_Float;
    //     return SetUniform(alVarId, data);
    // }

    // bool BGFXProgram::SetVec2f(int alVarId, float afX, float afY)
    // {
    //     UniformData data;
    //     data.input.v2[0] = afX;
    //     data.input.v2[1] = afY;
    //     data.type = UniformType_Vec2f;
    //     return SetUniform(alVarId, data);
    // }

    // bool BGFXProgram::SetVec3f(int alVarId, float afX, float afY, float afZ)
    // {
    //     UniformData data;
    //     data.input.v3[0] = afX;
    //     data.input.v3[1] = afY;
    //     data.input.v3[2] = afZ;
    //     data.type = UniformType_Vec3f;
    //     return SetUniform(alVarId, data);
    // }
    // bool BGFXProgram::SetVec4f(int alVarId, float afX, float afY, float afZ, float afW)
    // {
    //     UniformData data;
    //     data.input.v4[0] = afX;
    //     data.input.v4[1] = afY;
    //     data.input.v4[2] = afZ;
    //     data.input.v4[3] = afW;
    //     data.type = UniformType_Vec4f;
    //     return SetUniform(alVarId, data);
    // }
    // bool BGFXProgram::SetMatrixf(int alVarId, const cMatrixf& mMtx)
    // {
    //     UniformData data;
    //     std::copy(mMtx.v, mMtx.v + 16, data.input.m);
    //     data.type = UniformType_Matrixf;
    //     return SetUniform(alVarId, data);
    // }


    // bool BGFXProgram::SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp) { return false; }

    // bool BGFXProgram::CanAccessAPIMatrix()
    // {
    //     return true;
    // }
    
    // bool BGFXProgram::SetSamplerToUnit(const tString& asSamplerName, int alUnit)
    // {
    //     return true;
    // }

    // int BGFXProgram::GetVariableId(const tString& asName)
    // {
    //     if (_variableHandler)
    //     {
    //         return _variableHandler(asName);
    //     }
    //     return -1;
    // }

    // bool BGFXProgram::GetVariableAsId(const tString& asName, int alId)
    // {
    //     return GetVariableId(asName) == alId;
    // }

} // namespace hpl