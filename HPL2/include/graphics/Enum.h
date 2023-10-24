#pragma once

#include <cstdint>
#include "FixPreprocessor.h"
#include "graphics/GraphicsTypes.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "FixPreprocessor.h"

namespace hpl {

    enum class WrapMode : uint8_t {
        None,
        Mirror,
        Clamp,
        Edge,
        Border
    };

    enum class IndexBufferType : uint8_t {
        Uint16 = 0,
        Uint32 = 1
    };

    ShaderSemantic hplToForgeShaderSemantic(eVertexBufferElement element);
} // namespace hpl
