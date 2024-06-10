#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/offsetAllocator.h"
#include "graphics/GeometrySet.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/RingBuffer.h"
#include <folly/small_vector.h>
#include "math/Math.h"

namespace hpl {

    class GraphicsAllocator final {
        HPL_RTTI_CLASS(GraphicsAllocator , "{57396F88-B069-4CEB-AF7C-0A9419D165C3}")
    public:

        enum AllocationSet {
            OpaqueSet,
            ParticleSet,
            NumOfAllocationSets
        };

        GraphicsAllocator(ForgeRenderer* renderer);

        static constexpr uint32_t OpaqueVertexBufferSize = 6000000;
        static constexpr uint32_t OpaqueIndexBufferSize =  3000000;

        static constexpr uint32_t ParticleVertexBufferSize = 61440;
        static constexpr uint32_t ParticleIndexBufferSize = 61440;

        static constexpr uint32_t ImmediateVertexBufferSize = hpl::Math::BYTE_MB * 5;
        static constexpr uint32_t ImmediateIndexBufferSize =  hpl::Math::BYTE_MB * 3;

        GPURingBufferOffset allocTransientVertexBuffer(uint32_t size);
        GPURingBufferOffset allocTransientIndexBuffer(uint32_t size);
        GeometrySet& resolveSet(AllocationSet set);
    private:
        std::array<GeometrySet, NumOfAllocationSets> m_geometrySets;

        ForgeRenderer* m_renderer;
        GPURingBuffer m_transientVertexBuffer{};
        GPURingBuffer m_transientIndeciesBuffer{};
    };
} // namespace hpl
