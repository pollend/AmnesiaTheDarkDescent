#include "graphics/ForwardResources.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Material.h"

#include <cstdint>
#include <variant>

namespace hpl::resource  {

    LightResourceVariants CreateFromLight(iLight& light) {
        return std::monostate{};
    }

}
