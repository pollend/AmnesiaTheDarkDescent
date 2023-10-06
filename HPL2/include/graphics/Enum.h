#pragma once

#include "graphics/GraphicsTypes.h"
#include <cstdint>

namespace hpl
{

    enum class WrapMode : uint8_t {
        None,
        Mirror,
        Clamp,
        Edge,
        Border
    };


} // namespace hpl
