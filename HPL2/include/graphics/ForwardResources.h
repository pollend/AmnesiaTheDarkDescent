#pragma once

#include "graphics/GraphicsTypes.h"
#include "graphics/TextureDescriptorPool.h"
#include "scene/LightPoint.h"
#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>
#include <variant>

namespace hpl::resource {
    class iLight;
    struct PointLightResource {
        mat4 m_mvp;
        float3 m_lightPos;
        uint m_config;
        float4 m_lightColor;
        float m_radius;
    };
    using LightResourceVariants = std::variant<PointLightResource, std::monostate>;
    static LightResourceVariants CreateFromLight(iLight& light);

}
