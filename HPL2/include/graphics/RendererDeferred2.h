/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "engine/RTTI.h"

#include "graphics/CommandBufferPool.h"
#include "graphics/ShadowCache.h"
#include "scene/Viewport.h"
#include "scene/World.h"
#include "windowing/NativeWindow.h"

#include <graphics/ForgeHandles.h>
#include <graphics/ForgeRenderer.h>
#include <graphics/Image.h>
#include <graphics/Material.h>
#include <graphics/PassHBAOPlus.h>
#include <graphics/RenderList.h>
#include <graphics/RenderTarget.h>
#include <graphics/Renderable.h>
#include <graphics/Renderer.h>
#include <math/MathTypes.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>

#include <folly/small_vector.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hpl {

    class iFrameBuffer;
    class iDepthStencilBuffer;
    class iTexture;
    class iLight;
    class cSubMeshEntity;

    enum eDeferredShapeQuality {
        eDeferredShapeQuality_Low,
        eDeferredShapeQuality_Medium,
        eDeferredShapeQuality_High,
        eDeferredShapeQuality_LastEnum,
    };

    enum eDeferredSSAO {
        eDeferredSSAO_InBoxLight,
        eDeferredSSAO_OnColorBuffer,
        eDeferredSSAO_LastEnum,
    };

    class cRendererDeferred : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        static constexpr TinyImageFormat NormalBufferFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        static constexpr TinyImageFormat PositionBufferFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        static constexpr TinyImageFormat SpecularBufferFormat = TinyImageFormat_R8G8_UNORM;
        static constexpr TinyImageFormat ColorBufferFormat = TinyImageFormat_R8G8B8A8_UNORM;
        static constexpr TinyImageFormat ShadowDepthBufferFormat = TinyImageFormat_D32_SFLOAT;

        static constexpr uint32_t MaxReflectionBuffers = 4;
        static constexpr uint32_t MaxObjectUniforms = 4096;
        static constexpr uint32_t MaxLightUniforms = 1024;
        static constexpr uint32_t MaxHiZMipLevels = 10;
        static constexpr uint32_t MaxViewportFrameDescriptors = 256;
        static constexpr uint32_t MaxMaterialSamplers = static_cast<uint32_t>(eTextureWrap_LastEnum) * static_cast<uint32_t>(eTextureFilter_LastEnum) * static_cast<uint32_t>(cMaterial::TextureAntistropy::Antistropy_Count);
        static constexpr uint32_t MaxObjectTest = 32768;
        static constexpr uint32_t MaxOcclusionDescSize = 4096;
        static constexpr uint32_t MaxQueryPoolSize = MaxOcclusionDescSize * 2;
        static constexpr uint32_t MaxIndirectDrawArgs = 4096;

        static constexpr uint32_t MaxDiffuseMaterials = 512;

        static constexpr uint32_t TranslucencyBlendModeMask = 0xf;

        static constexpr uint32_t TranslucencyReflectionBufferMask = 0x7;
        static constexpr uint32_t TranslucencyReflectionBufferOffset = 4;

        static constexpr float ShadowDistanceMedium = 10;
        static constexpr float ShadowDistanceLow = 20;
        static constexpr float ShadowDistanceNone = 40;

    private:
        SharedRootSignature m_shadowClearRootSignature;
        SharedShader m_shaderShadowClear;
        SharedPipeline m_shadowClearPipeline;

        struct SceneMaterial {
        public:
            void* m_material = nullptr;
            uint32_t m_version = 0;
            IndexPoolHandle m_slot;
        };

        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_indirectDrawArgsBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_perSceneBuffer;
    };
}; // namespace hpl

