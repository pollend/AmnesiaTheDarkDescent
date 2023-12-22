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
#include "graphics/ImageBindlessPool.h"
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

        static constexpr uint32_t MaxReflectionBuffers = 4;
        static constexpr uint32_t MaxObjectUniforms = 4096;
        static constexpr uint32_t MaxLightUniforms = 1024;
        static constexpr uint32_t MaxDecalUniforms = 1024;
        static constexpr uint32_t MaxParticleUniform = 1024;
        static constexpr uint32_t MaxIndirectDrawElements = 4096;

        static constexpr uint32_t PointLightCount = 256;
        static constexpr float ShadowDistanceMedium = 10;
        static constexpr float ShadowDistanceLow = 20;
        static constexpr float ShadowDistanceNone = 40;

        static constexpr uint32_t MaxSolidDiffuseMaterials = 512;

        static constexpr uint32_t LightClusterWidth = 16;
        static constexpr uint32_t LightClusterHeight = 9;
        static constexpr uint32_t LightClusterSlices = 24;
        static constexpr uint32_t LightClusterLightCount = 128;
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
            ViewportData& operator=(const ViewportData&) = delete;
            ViewportData& operator=(ViewportData&& buffer) = default;

            uint2 m_size = uint2(0, 0);
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_outputBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_depthBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_albedoBuffer; // this is used for the adding decals to albedo

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
            const ForgeRenderer::Frame& frame,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings,
            bool abSendFrameBufferToPostEffects) override;

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

        struct SharedMaterial {
        public:
            SharedMaterial();
            void* m_material = nullptr;
            uint32_t m_version = 0;
            folly::small_vector<MaterialSet, 2> m_sets;
            std::array<uint32_t, eMaterialTexture_LastEnum> m_textureHandles;
            MaterialSet& resolveSet(MaterialSetType set);
        };
        SharedMaterial& resolveSharedMaterial(cMaterial* material);
        uint32_t resolveObjectIndex(const ForgeRenderer::Frame& frame,
            iRenderable* apObject,
            std::optional<Matrix4> modelMatrix);

        UniqueViewportData<ViewportData> m_boundViewportData;

        SharedSampler m_samplerNearEdgeClamp;
        SharedSampler m_samplerPointWrap;
        SharedSampler m_samplerPointClampToBorder;
        std::array<SharedMaterial, cMaterial::MaxMaterialID> m_sharedMaterial;
        TextureDescriptorPool m_sceneTexture2DPool;
        ImageBindlessPool m_sceneTransientImage2DPool;
        CommandSignature* m_cmdSignatureVBPass = NULL;

        // diffuse
        IndexPool m_diffuseIndexPool;
        SharedBuffer m_diffuseSolidMaterialUniformBuffer;

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
        folly::F14ValueMap<iRenderable*, uint32_t> m_objectDescriptorLookup;

        SharedTexture m_emptyTexture;
        Image* m_dissolveImage;

        uint32_t m_activeFrame = 0;
        uint32_t m_objectIndex = 0;
        uint32_t m_indirectDrawIndex = 0;

        cRenderList m_rendererList;

        // Lights
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

        SharedShader m_particleShaderAdd;
        SharedShader m_particleShaderMul;
        SharedShader m_particleShaderMulX2;
        SharedShader m_particleShaderAlpha;
        SharedShader m_particleShaderPremulAlpha;

        SharedPipeline m_particleBlendAdd;
        SharedPipeline m_particleBlendAddNoDepth;

        SharedPipeline m_particleBlendMul;
        SharedPipeline m_particleBlendMulNoDepth;

        SharedPipeline m_particleBlendMulX2;
        SharedPipeline m_particleBlendMulX2NoDepth;

        SharedPipeline m_particleBlendAlpha;
        SharedPipeline m_particleBlendAlphaNoDepth;

        SharedPipeline m_particleBlendPremulAlpha;
        SharedPipeline m_particleBlendPremulAlphaNoDepth;

        SharedShader m_decalShader;
        SharedPipeline m_decalPipelineAdd;
        SharedPipeline m_decalPipelineMul;
        SharedPipeline m_decalPipelineMulX2;
        SharedPipeline m_decalPipelineAlpha;
        SharedPipeline m_decalPipelinePremulAlpha;

        Queue* m_computeQueue = nullptr;
        GpuCmdRing m_computeRing = {};

        bool m_supportIndirectRootConstant = false;
    };
}; // namespace hpl

