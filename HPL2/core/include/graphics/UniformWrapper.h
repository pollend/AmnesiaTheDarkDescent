#pragma once

#include "bgfx/bgfx.h"
#include "resources/StringLiteral.h"
#include <memory>

namespace hpl {

    template<StringLiteral id, bgfx::UniformType::Enum type>
    class UniformWrapper {
    public:
        UniformWrapper() {
            // m_handle = bgfx::createUniform(id.m_str, type);
        }

        void Initialize() {
            if(!bgfx::isValid(m_handle)) {
                m_handle = bgfx::createUniform(id.m_str, type);
            }
        }

        ~UniformWrapper() {
            if(bgfx::isValid(m_handle)) {
                bgfx::destroy(m_handle);
                m_handle = BGFX_INVALID_HANDLE;
            }
        }

        operator bgfx::UniformHandle() {
            return m_handle;
        }
    private:
        bgfx::UniformHandle m_handle = BGFX_INVALID_HANDLE;
    };
}