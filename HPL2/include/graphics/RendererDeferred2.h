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
#include "graphics/CopyTextureSubpass4.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/ImageBindlessPool.h"
#include "graphics/SceneResource.h"
#include "graphics/ScopedBarrier.h"
#include "graphics/ShadowCache.h"
#include "graphics/BindlessDescriptorPool.h"
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

namespace hpl {

    class iFrameBuffer;
    class iDepthStencilBuffer;
    class iTexture;
    class iLight;
    class cSubMeshEntity;

    class cRendererDeferred2 : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{B3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
        struct RangeSubsetAlloc {
        public:
            struct RangeSubset {
                uint32_t m_start = 0;
                uint32_t m_end = 0;
                inline uint32_t size() {
                    return m_end - m_start;
                }
            };
            uint32_t& m_index;
            uint32_t m_start;
            RangeSubsetAlloc(uint32_t& index)
                : m_index(index)
                , m_start(index) {
            }
            uint32_t Increment() {
                return (m_index++);
            }
            RangeSubset End() {
                uint32_t start = m_start;
                m_start = m_index;
                return RangeSubset{ start, m_index };
            }
        };

        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT;
        static constexpr TinyImageFormat NormalBufferFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        static constexpr TinyImageFormat PositionBufferFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        static constexpr TinyImageFormat SpecularBufferFormat = TinyImageFormat_R8G8_UNORM;
        static constexpr TinyImageFormat ColorBufferFormat = TinyImageFormat_R8G8B8A8_UNORM;
        static constexpr TinyImageFormat ShadowDepthBufferFormat = TinyImageFormat_D32_SFLOAT;

        static constexpr float ShadowDistanceMedium = 10;
        static constexpr float ShadowDistanceLow = 20;
        static constexpr float ShadowDistanceNone = 40;

        static constexpr uint32_t LightClusterWidth = 16;
        static constexpr uint32_t LightClusterHeight = 9;
        static constexpr uint32_t LightClusterSlices = 24;
        static constexpr uint32_t TransientImagePoolCount = 256;

        static constexpr uint32_t IndirectArgumentSize = 8 * sizeof(uint32_t);

        struct RootConstantDrawIndexArguments {
            uint32_t mDrawId;
            uint32_t mIndexCount;
            uint32_t mInstanceCount;
            uint32_t mStartIndex;
            uint32_t mVertexOffset;
            uint32_t mStartInstance;
        };

        struct ViewportData {
        public:
            ViewportData() = default;
            ViewportData(const ViewportData&) = delete;
            ViewportData(ViewportData&& buffer)= default;
            ViewportData& operator=(ViewportData&& buffer) = default;
            ViewportData& operator=(const ViewportData&) = delete;

            uint2 m_size = uint2(0, 0);
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_outputBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_depthBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_albedoBuffer; // this is used for the adding decals to albedo
            std::array<SharedTexture, ForgeRenderer::SwapChainLength> m_refractionImage;

            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_testBuffer; //encodes the parallax
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_visiblityBuffer;
        };

         struct FogRendererData {
            cFogArea* m_fogArea;
            bool m_insideNearFrustum;
            cVector3f m_boxSpaceFrustumOrigin;
            cMatrixf m_mtxInvBoxSpace;
         };

        cRendererDeferred2(
            cGraphics* apGraphics,
            cResources* apResources,
            std::shared_ptr<DebugDraw> debug);
        virtual ~cRendererDeferred2();

        virtual SharedRenderTarget GetOutputImage(uint32_t frameIndex, cViewport& viewport) override;
        virtual void Draw(
            Cmd* cmd,
            ForgeRenderer::Frame& frame,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings) override;

    private:
        void setIndirectDrawArg(const ForgeRenderer::Frame& frame, uint32_t drawArgIndex, uint32_t slot, DrawPacket& packet);

        enum MaterialSetType {
            PrimarySet = 0,
            ParticleSet = 1
        };
        struct MaterialSet {
            MaterialSetType m_type;
            IndexPoolHandle m_slot;
        };

        struct ResourceMaterial {
        public:
            ResourceMaterial();
            void* m_material = nullptr;
            uint32_t m_version = 0;
            folly::small_vector<MaterialSet, 2> m_sets;
            std::array<uint32_t, eMaterialTexture_LastEnum> m_textureHandles;
            MaterialSet& resolveSet(MaterialSetType set);
        };
        ResourceMaterial& resolveResourceMaterial(cMaterial* material);
        uint32_t resolveObjectSlot(uint32_t uid, std::function<void(uint32_t)> initializeHandler);

        UniqueViewportData<ViewportData> m_boundViewportData;

        SharedSampler m_samplerNearEdgeClamp;
        SharedSampler m_samplerPointWrap;
        SharedSampler m_samplerPointClampToBorder;
        SharedSampler m_samplerMaterial;
        SharedSampler m_samplerLinearClampToBorder;
        std::array<ResourceMaterial, cMaterial::MaxMaterialID> m_sharedMaterial;
        BindlessDescriptorPool m_sceneTexture2DPool;
        BindlessDescriptorPool m_sceneTextureCubePool;
        ImageBindlessPool m_sceneTransientImage2DPool;
        CommandSignature* m_cmdSignatureVBPass = NULL;

        // diffuse
        IndexPool m_diffuseIndexPool;
        IndexPool m_translucencyIndexPool;
        IndexPool m_waterIndexPool;

        SharedRootSignature m_sceneRootSignature;
        SharedDescriptorSet m_sceneDescriptorConstSet;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_sceneDescriptorPerFrameSet;

        SharedShader m_visibilityBufferPassShader;
        SharedShader m_visibilityBufferAlphaPassShader;
        SharedShader m_visibilityBufferEmitPassShader;
        SharedShader m_visibilityShadePassShader;

        SharedPipeline m_visibilityBufferPass;
        SharedPipeline m_visbilityAlphaBufferPass;
        SharedPipeline m_visbilityEmitBufferPass;
        SharedPipeline m_visiblityShadePass;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_objectUniformBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_perSceneInfoBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_indirectDrawArgsBuffer;

        std::array<SharedSampler, resource::MaterialSceneSamplersCount> m_materialSampler;
        folly::F14ValueMap<uint32_t, uint32_t> m_objectDescriptorLookup;

        SharedTexture m_emptyTexture2D;
        SharedTexture m_emptyTextureCube;
        Image* m_dissolveImage;

        uint32_t m_activeFrame = 0;
        uint32_t m_objectIndex = 0;
        uint32_t m_indirectDrawIndex = 0;

        cRenderList m_rendererList;

        SharedRootSignature m_lightClusterRootSignature;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_lightDescriptorPerFrameSet;
        SharedShader m_lightClusterShader;
        SharedShader m_clearLightClusterShader;
        SharedPipeline m_lightClusterPipeline;
        SharedPipeline m_clearClusterPipeline;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_lightClustersBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_lightClusterCountBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_lightBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_particleBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_decalBuffer;
        SharedBuffer m_translucencyMatBuffer;
        SharedBuffer m_waterMatBuffer;
        SharedBuffer m_diffuseMatUniformBuffer;

        SharedShader m_particleShaderAdd;
        SharedShader m_particleShaderMul;
        SharedShader m_particleShaderMulX2;
        SharedShader m_particleShaderAlpha;
        SharedShader m_particleShaderPremulAlpha;

        SharedShader m_translucencyShaderAdd;
        SharedShader m_translucencyShaderMul;
        SharedShader m_translucencyShaderMulX2;
        SharedShader m_translucencyShaderAlpha;
        SharedShader m_translucencyShaderPremulAlpha;

        SharedShader m_translucencyRefractionShaderAdd;
        SharedShader m_translucencyRefractionShaderMul;
        SharedShader m_translucencyRefractionShaderMulX2;
        SharedShader m_translucencyRefractionShaderAlpha;
        SharedShader m_translucencyRefractionShaderPremulAlpha;

        SharedShader m_translucencyIlluminationShaderAdd;
        SharedShader m_translucencyIlluminationShaderMul;
        SharedShader m_translucencyIlluminationShaderMulX2;
        SharedShader m_translucencyIlluminationShaderAlpha;
        SharedShader m_translucencyIlluminationShaderPremulAlpha;

        struct DisableEnableDepthPipelines {
            SharedPipeline m_enableDepth;
            SharedPipeline m_disableDepth;
        };

        struct BlendPipelines {
            SharedPipeline m_pipelineBlendAdd;
            SharedPipeline m_pipelineBlendMul;
            SharedPipeline m_pipelineBlendMulX2;
            SharedPipeline m_pipelineBlendAlpha;
            SharedPipeline m_pipelineBlendPremulAlpha;
            SharedPipeline& getPipelineByBlendMode(eMaterialBlendMode mode) {
                switch (mode) {
                case eMaterialBlendMode_None:
                case eMaterialBlendMode_Add:
                    return m_pipelineBlendAdd;
                case eMaterialBlendMode_Mul:
                    return m_pipelineBlendMul;
                case eMaterialBlendMode_MulX2:
                    return m_pipelineBlendMulX2;
                case eMaterialBlendMode_Alpha:
                    return m_pipelineBlendAlpha;
                case eMaterialBlendMode_PremulAlpha:
                    return m_pipelineBlendPremulAlpha;
                default:
                    break;
                }
                return m_pipelineBlendAdd;
            }
        };
        BlendPipelines m_particlePipeline;
        BlendPipelines m_particlePipelineNoDepth;

        SharedShader m_decalShader;
        BlendPipelines m_decalPipelines;

        BlendPipelines m_translucencyPipline;
        BlendPipelines m_translucencyPiplineNoDepth;

        BlendPipelines m_translucencyRefractionPipline;
        BlendPipelines m_translucencyRefractionPiplineNoDepth;

        BlendPipelines m_translucencyIlluminationPipline;
        BlendPipelines m_translucencyIlluminationPiplineNoDepth;

        CopyTextureSubpass4 m_copySubpass;

        bool m_supportIndirectRootConstant = false;
    };
}; // namespace hpl

