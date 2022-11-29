#pragma once

#include <graphics/GraphicsContext.h>
#include <graphics/HPLShaderDefinition.h>
#include <bgfx/bgfx.h>
#include <functional>
#include <graphics/GPUProgram.h>

#include <bx/debug.h>
#include <graphics/HPLShaderDefinition.h>

namespace hpl
{

    // this is wrapper around GpuProgram until a replacement is found
    template<typename TData>
    class BGFXProgram : public iGpuProgram
    {
    public:
        using SubmitHandler = std::function<void(const TData& data, bgfx::ViewId view, GraphicsContext& context)>;
        
        BGFXProgram(
            hpl::shader::definition::HPLShaderDefinition<TData>&& definition, 
            const tString& asName, 
            eGpuProgramFormat aProgramFormat): iGpuProgram(asName, aProgramFormat), _definition(definition) {
        }
        virtual ~BGFXProgram() {

        }

        BGFXProgram* SetSubmitHandler(SubmitHandler handler) {
            _submitHandler = handler;
            return this;
        }

		virtual void Submit(bgfx::ViewId view, GraphicsContext& context) override {

        }
        virtual bool Link() override{
            return false;
        }
        virtual void Bind() override{
            
        }
        virtual void UnBind() override{
            
        }

        virtual bool CanAccessAPIMatrix() override{
            return true;
        }
		virtual bool SetSamplerToUnit(const tString& asSamplerName, int alUnit) override{
            return true;
        }
		virtual int GetVariableId(const tString& asName) override{
            return true;
        }
		virtual bool GetVariableAsId(const tString& asName, int alId) override{
            return true;
        }

        virtual bool SetInt(int alVarId, int alX) override {
            for(auto& uniform : _definition.Uniforms()) {
                if(uniform.getId().id() == alVarId) {
                    uniform.template Set<shader::definition::HPLParameterType::HPL_PARAMETER_INT>(_data, alX);
                }
            }
            return false;
        }
        virtual bool SetFloat(int alVarId, float afX) override{
            return true;
        }
        virtual bool SetVec2f(int alVarId, float afX, float afY) override{
            return true;
        }
        virtual bool SetVec3f(int alVarId, float afX, float afY, float afZ) override{
            return true;
        }
        virtual bool SetVec4f(int alVarId, float afX, float afY, float afZ, float afW) override{
            return true;
        }
        virtual bool SetMatrixf(int alVarId, const cMatrixf& mMtx) override{
            return true;
        }
		virtual bool SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp) override{
            return true;
        }

    private:
        TData _data;
        SubmitHandler _submitHandler;
        // UniformDataHandler _uniformHandler;
        // VariableDataHandler _variableHandler;
        hpl::shader::definition::HPLShaderDefinition<TData> _definition;
        bgfx::ProgramHandle _handler;
    };

} // namespace hpl