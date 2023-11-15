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
#include "graphics/SceneResource.h"
#include "graphics/ShadowCache.h"
#include "graphics/TextureDescriptorPool.h"
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

    class cRendererDeferred2 : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{B3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
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
        static constexpr uint32_t MaxObjectTest = 32768;
        static constexpr uint32_t MaxOcclusionDescSize = 4096;
        static constexpr uint32_t MaxQueryPoolSize = MaxOcclusionDescSize * 2;
        static constexpr uint32_t MaxIndirectDrawArgs = 4096;

        static constexpr uint32_t TranslucencyBlendModeMask = 0xf;

        static constexpr uint32_t TranslucencyReflectionBufferMask = 0x7;
        static constexpr uint32_t TranslucencyReflectionBufferOffset = 4;

        static constexpr float ShadowDistanceMedium = 10;
        static constexpr float ShadowDistanceLow = 20;
        static constexpr float ShadowDistanceNone = 40;

        static constexpr uint32_t MaxSolidDiffuseMaterials = 512;

        struct ViewportData {
        public:
            ViewportData() = default;
            ViewportData(const ViewportData&) = delete;
            ViewportData(ViewportData&& buffer)= default;
            ViewportData& operator=(const ViewportData&) = delete;
            ViewportData& operator=(ViewportData&& buffer) = default;

            uint2 m_size = uint2(0, 0);
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_outputBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_depthBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_visiblityBuffer;
        };

        cRendererDeferred2(
            cGraphics* apGraphics,
            cResources* apResources,
            std::shared_ptr<DebugDraw> debug);
        virtual ~cRendererDeferred2();

        virtual SharedRenderTarget GetOutputImage(uint32_t frameIndex, cViewport& viewport) override;
        virtual void Draw(
            Cmd* cmd,
            const ForgeRenderer::Frame& frame,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings,
            bool abSendFrameBufferToPostEffects) override;

    private:

        struct SceneMaterial {
        public:
            void* m_material = nullptr;
            uint32_t m_version = 0;
            IndexPoolHandle m_slot;
            resource::MaterialTypes m_resource = std::monostate{};
        };
        cRendererDeferred2::SceneMaterial& resolveMaterial(const ForgeRenderer::Frame& frame,cMaterial* material);
        uint32_t resolveObjectIndex(
            const ForgeRenderer::Frame& frame, iRenderable* apObject, std::optional<Matrix4> modelMatrix);

        UniqueViewportData<ViewportData> m_boundViewportData;

        std::array<SceneMaterial, cMaterial::MaxMaterialID> m_sceneMaterial;
        TextureDescriptorPool m_sceneTexture2DPool;

        CommandSignature* m_cmdSignatureVBPass = NULL;

        SharedShader m_visibilityPassShader;

        // diffuse
        IndexPool m_diffuseIndexPool;
        SharedBuffer m_diffuseSolidMaterialUniformBuffer;

        SharedRootSignature m_sceneRootSignature;
        SharedDescriptorSet m_sceneDescriptorConstSet;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_sceneDescriptorPerFrameSet;

        SharedPipeline m_visibilityPass;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_objectUniformBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_perSceneInfoBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_indirectDrawArgsBuffer;

        std::array<SharedSampler, resource::MaterialSceneSamplersCount> m_materialSampler;
        folly::F14ValueMap<iRenderable*, uint32_t> m_objectDescriptorLookup;

        SharedTexture m_emptyTexture;

        uint32_t m_activeFrame = 0;
        uint32_t m_objectIndex = 0;
        uint32_t m_indirectDrawIndex = 0;

        cRenderList m_rendererList;
    };
}; // namespace hpl

