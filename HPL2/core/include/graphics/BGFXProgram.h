#pragma once

#include "graphics/Image.h"
#include "math/MathTypes.h"
#include <algorithm>
#include <bgfx/bgfx.h>
#include <functional>
#include <graphics/GPUProgram.h>
#include <graphics/GraphicsContext.h>
#include <graphics/MemberID.h>

#include <bx/debug.h>
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
        using ImageMapper = std::function<void(TData& data,const Image* value)>;
        using IntMapper = std::function<void(TData& data, int value)>;
        using FloatMapper = std::function<void(TData& data, float value)>;
        using BoolMapper = std::function<void(TData& data, bool value)>;
        using Vec2Mapper = std::function<void(TData& data, float afx, float afy)>;
        using Vec3Mapper = std::function<void(TData& data, float afx, float afy, float afz)>;
        using Vec4Mapper = std::function<void(TData& data, float afx, float afy, float afz, float afw)>;
        using Matrix4fMapper = std::function<void(TData& data, cMatrixf mat)>;

        class ParameterField
        {
        public:
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


            explicit ParameterField(const MemberID id, ImageMapper mapper)
                : _member(id)
                , _imageMapper(mapper)
            {
            }

            const MemberID& member()
            {
                return _member;
            }

        private:
            MemberID _member = MemberID();
            IntMapper _intMapper = nullptr;
            ImageMapper _imageMapper = nullptr;
            FloatMapper _floatMapper = nullptr;
            BoolMapper _boolMapper = nullptr;
            Vec2Mapper _vec2Mapper = nullptr;
            Vec3Mapper _vec3Mapper = nullptr;
            Vec4Mapper _vec4Mapper = nullptr;
            Matrix4fMapper _matMapper = nullptr;

            friend class BGFXProgram;
        };

        BGFXProgram(
            std::vector<ParameterField>&& fields, const tString& asName, bgfx::ProgramHandle programHandle, bool destroyProgram, eGpuProgramFormat aProgramFormat)
            : iGpuProgram(asName, aProgramFormat)
            , _fields(fields)
            , _handle(programHandle)
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
        
        TData& data() {
            return _data;
        }
        
        virtual void Submit(bgfx::ViewId view, GraphicsContext& context) override
        {
            bgfx::submit(view, _handle);
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

		virtual bool setImage(const MemberID& id, const Image* image) override { 
            if(!image) {
                return false;
            }

            return _setterById(
                id.id(),
                [&](ParameterField& field)
                {
                    if (field._imageMapper)
                    {
                        field._imageMapper(_data, image);
                        return true;
                    }
                    return false;
                });
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
        bgfx::ProgramHandle _handle;
    };

} // namespace hpl