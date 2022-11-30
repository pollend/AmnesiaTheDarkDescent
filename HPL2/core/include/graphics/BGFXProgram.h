#pragma once

#include "math/MathTypes.h"
#include <algorithm>
#include <bgfx/bgfx.h>
#include <functional>
#include <graphics/GPUProgram.h>
#include <graphics/GraphicsContext.h>
#include <graphics/HPLShaderDefinition.h>

#include <bx/debug.h>
#include <graphics/HPLShaderDefinition.h>
#include <optional>
#include <vector>

namespace hpl
{

    // this is wrapper around GpuProgram until a replacement is found
    template<typename TData>
    class BGFXProgram : public iGpuProgram
    {
    public:
        using SubmitHandler = std::function<void(const TData& data, bgfx::ViewId view, GraphicsContext& context)>;

        class MemberID
        {
        public:
            MemberID(const tString& asName, int id)
                : _id(id)
                , _name(name)
            {
            }

            MemberID(): 
                _id(-1),
                _name() {

                }

            const tString& name() const
            {
                return _name;
            }

            int id() const
            {
                return _id;
            }

        private:
            int _id;
            tString _name;
        };

        class ParameterField
        {
        public:
            using IntMapper = std::function<bool(TData& data, int value)>;
            using FloatMapper = std::function<bool(TData& data, float value)>;
            using BoolMapper = std::function<bool(TData& data, bool value)>;
            using Vec2Mapper = std::function<bool(TData& data, float afx, float afy)>;
            using Vec3Mapper = std::function<bool(TData& data, float afx, float afy, float afz)>;
            using Vec4Mapper = std::function<bool(TData& data, float afx, float afy, float afz, float afw)>;
            using Matrix4fMapper = std::function<bool(TData& data, cMatrixf mat)>;

            explicit ParameterField(const MemberID id, IntMapper mapper)
                : _member(id)
                , _intMapper(mapper)
            {
            }

            explicit ParameterField(const MemberID id, FloatMapper mapper)
                : _member(id)
                , _floatMapper(mapper)
            {
            }
            
            explicit ParameterField(const MemberID id, BoolMapper mapper)
                : _member(id)
                , _boolMapper(mapper)
            {
            }
            
            explicit ParameterField(const MemberID id, Vec2Mapper mapper)
                : _member(id)
                , _vec2Mapper(mapper)
            {
            }
            
            explicit ParameterField(const MemberID id, Vec3Mapper mapper)
                : _member(id)
                , _vec3Mapper(mapper)
            {
            }

            explicit ParameterField(const MemberID id, Vec4Mapper mapper)
                : _member(id)
                , _vec4Mapper(mapper)
            {
            }

            explicit ParameterField(const MemberID id, Matrix4fMapper mapper)
                : _member(id)
                , _matMapper(mapper)
            {
            }

            const MemberID& member()
            {
                return _member;
            }

        private:
            MemberID _member = MemberID();
            IntMapper _intMapper = nullptr;
            FloatMapper _floatMapper = nullptr;
            BoolMapper _boolMapper = nullptr;
            Vec2Mapper _vec2Mapper = nullptr;
            Vec3Mapper _vec3Mapper = nullptr;
            Vec4Mapper _vec4Mapper = nullptr;
            Matrix4fMapper _matMapper = nullptr;

            friend class BGFXProgram;
        };

        BGFXProgram(
            hpl::shader::definition::HPLShaderDefinition<TData>&& definition, const tString& asName, eGpuProgramFormat aProgramFormat)
            : iGpuProgram(asName, aProgramFormat)
            , _definition(definition)
        {
        }

        virtual ~BGFXProgram()
        {
        }

        BGFXProgram* SetSubmitHandler(SubmitHandler handler)
        {
            _submitHandler = handler;
            return this;
        }

        virtual void Submit(bgfx::ViewId view, GraphicsContext& context) override
        {
        }
        virtual bool Link() override
        {
            return false;
        }
        virtual void Bind() override
        {
        }
        virtual void UnBind() override
        {
        }

        virtual bool SetSamplerToUnit(const tString& asSamplerName, int alUnit) override
        {
            return true;
        }
        virtual int GetVariableId(const tString& asName) override
        {
            for (auto& field : _fields)
            {
                auto& member = field.member();
                if (member.name() == asName)
                {
                    return member.id();
                }
            }
            return -1;
        }
        virtual bool GetVariableAsId(const tString& asName, int alId) override
        {
            return true;
        }

        virtual bool SetInt(int alVarId, int alX) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._floatMapper)
                    {
                        field._intMapper(_data, alX);
                        return true;
                    }
                    return false;
                });
        }

        virtual bool SetFloat(int alVarId, float afX) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._floatMapper)
                    {
                        field._floatMapper(_data, afX);
                        return true;
                    }
                    return false;
                });
        }
        virtual bool SetVec2f(int alVarId, float afX, float afY) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._vec2Mapper)
                    {
                        field._vec2Mapper(_data, afX, afY);
                        return true;
                    }
                    return false;
                });
        }
        virtual bool SetVec3f(int alVarId, float afX, float afY, float afZ) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._vec3Mapper)
                    {
                        field._vec3Mapper(_data, afX, afY, afZ);
                        return true;
                    }
                    return false;
                });
        }
        virtual bool SetVec4f(int alVarId, float afX, float afY, float afZ, float afW) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._vec4Mapper)
                    {
                        field._vec4Mapper(_data, afX, afY, afZ, afW);
                        return true;
                    }
                    return false;
                });
        }
        virtual bool SetMatrixf(int alVarId, const cMatrixf& mMtx) override
        {
            return _setterById(
                alVarId,
                [&](ParameterField& field)
                {
                    if (field._matMapper)
                    {
                        field._matMapper(_data, mMtx);
                        return true;
                    }
                    return false;
                });
        }

        virtual bool SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp) override
        {
            return true;
        }

    private:
        bool _setterById(int varId, std::function<bool(ParameterField&)> handler)
        {
            for (auto& field : _fields)
            {
                if (field.member().id() == varId)
                {
                    return handler(field);
                }
            }
            return false;
        }

        TData _data;
        SubmitHandler _submitHandler;
        std::vector<ParameterField> _fields;
        hpl::shader::definition::HPLShaderDefinition<TData> _definition;
        bgfx::ProgramHandle _handler;
    };

} // namespace hpl