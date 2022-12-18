#pragma once

#include <absl/strings/string_view.h>
#include <bgfx/bgfx.h>
#include <cstdint>

namespace hpl
{
    class GraphicsContext final
    {
    public:
        GraphicsContext();
        void Init();
        void UpdateScreenSize(uint16_t width, uint16_t height);

        void ScreenSpaceQuad(float textureWidth, float textureHeight, float width = 1.0f, float height = 1.0f);

        uint16_t ScreenWidth() const;
        uint16_t ScreenHeight() const;

        void Reset();
        bgfx::ViewId StartPass(absl::string_view name);
        bool isOriginBottomLeft() const;

    private:
        bgfx::ViewId _current;
        const bgfx::Caps* _caps;
    };
} // namespace hpl
