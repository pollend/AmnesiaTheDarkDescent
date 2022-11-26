#pragma once

#include "graphics/GraphicsContext.h"
#include <bgfx/bgfx.h>
#include <functional>
#include <graphics/GPUProgram.h>

#include <bx/debug.h>

namespace hpl
{

    // this is wrapper around GpuProgram until a replacement is found
    class BGFXProgram : public iGpuProgram
    {
    public:
        
        enum UniformType
        {
            UniformType_Int,
            UniformType_Float,
            UniformType_Vec2f,
            UniformType_Vec3f,
            UniformType_Vec4f,
            UniformType_Matrixf
        };

        struct UniformData
        {
            UniformType type;
            union {
                int i;
                float f;
                float v2[2];
                float v3[3];
                float v4[4];
                float m[16];
            } input;
        };

        using SubmitHandler = std::function<void(bgfx::ViewId view, GraphicsContext& context)>;
        using UniformDataHandler = std::function<bool(int varId, const UniformData&)>;
        using VariableDataHandler = std::function<int(const tString& asName)>;

		BGFXProgram(const tString& asName,eGpuProgramFormat aProgramFormat);
		virtual ~BGFXProgram();


        BGFXProgram& SetSubmitHandler(SubmitHandler handler);
        BGFXProgram& SetUniformHandler(UniformDataHandler handler);
        BGFXProgram& SetVariableHandler(VariableDataHandler handler);

        bool SetUniform(int varId, const UniformData& data);

		virtual void Submit(bgfx::ViewId view, GraphicsContext& context) override;
        virtual bool Link() override;
        virtual void Bind() override;
        virtual void UnBind() override;

        virtual bool CanAccessAPIMatrix() override;
		virtual bool SetSamplerToUnit(const tString& asSamplerName, int alUnit) override;
		virtual int GetVariableId(const tString& asName) override;
		virtual bool GetVariableAsId(const tString& asName, int alId) override;

        virtual bool SetInt(int alVarId, int alX) override;
        virtual bool SetFloat(int alVarId, float afX) override;
        virtual bool SetVec2f(int alVarId, float afX, float afY) override;
        virtual bool SetVec3f(int alVarId, float afX, float afY, float afZ) override;
        virtual bool SetVec4f(int alVarId, float afX, float afY, float afZ, float afW) override;
        virtual bool SetMatrixf(int alVarId, const cMatrixf& mMtx) override;
		virtual bool SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp) override;

    private:
        SubmitHandler _submitHandler;
        UniformDataHandler _uniformHandler;
        VariableDataHandler _variableHandler;
        bgfx::ProgramHandle _handler;
    };

} // namespace hpl