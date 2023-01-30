

#pragma once

#include <bgfx/bgfx.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <graphics/ShaderUtil.h>
#include <iterator>
#include <string_view>

namespace hpl
{
    struct ShaderVariant
    {
        bgfx::ProgramHandle m_programHandle;
        std::function<void(ShaderVariant& variant)> m_disposeHandler = [](ShaderVariant& variant) {};
    };

    namespace ShaderHelper {
        std::function<ShaderVariant(uint32_t flags)> LoadProgramHandlerDefault(std::string_view vs, std::string_view fs, bool isVariantVertex = true, bool isVariantFragment = true);
    }

    template<uint32_t TSize>
    class ShaderVariantCollection final
    {
    public:
        ShaderVariantCollection();
        ~ShaderVariantCollection();

        void Initialize(std::function<ShaderVariant(uint32_t flags)>);
        bgfx::ProgramHandle GetVariant(uint32_t flag) const;

    private:
        std::array<ShaderVariant, TSize + 1> m_variants;
    };

} // namespace hpl

namespace hpl
{

    template<uint32_t TSize>
    ShaderVariantCollection<TSize>::ShaderVariantCollection() {
        ShaderVariant defaultShader = { BGFX_INVALID_HANDLE, [](ShaderVariant& variant) {
                                       } };
        m_variants.fill(defaultShader);
    }

    template<uint32_t TSize>
    ShaderVariantCollection<TSize>::~ShaderVariantCollection()
    {
        for (uint32_t i = 0; i <= TSize; ++i)
        {
            m_variants[i].m_disposeHandler(m_variants[i]);
        }
    }

    template<uint32_t TSize>
    bgfx::ProgramHandle ShaderVariantCollection<TSize>::GetVariant(uint32_t flag) const
    {
        BX_ASSERT(flag <= TSize, "Invalid flag %d", flag)
        return m_variants[flag].m_programHandle;
    }

    template<uint32_t TSize>
    void ShaderVariantCollection<TSize>::Initialize(std::function<ShaderVariant(uint32_t flags)> handler)
    {
        for (uint32_t i = 0; i <= TSize; ++i)
        {
            if (bgfx::isValid(m_variants[i].m_programHandle))
            {
                bgfx::destroy(m_variants[i].m_programHandle);
            }
            m_variants[i] = handler(i);
        }
    }
} // namespace hpl
