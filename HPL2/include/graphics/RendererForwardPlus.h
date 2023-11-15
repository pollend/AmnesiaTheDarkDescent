
#pragma once

#include "engine/RTTI.h"

#include "graphics/ForwardResources.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/IndexPool.h"
#include "graphics/TextureDescriptorPool.h"
#include "graphics/offsetAllocator.h"
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
#include <graphics/SceneResource.h>
#include <math/MathTypes.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>



namespace hpl {
    class RendererForwardPlus : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        static constexpr TinyImageFormat NormalBufferFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        static constexpr TinyImageFormat PositionBufferFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        static constexpr TinyImageFormat SpecularBufferFormat = TinyImageFormat_R8G8_UNORM;
        static constexpr TinyImageFormat ColorBufferFormat = TinyImageFormat_R8G8B8A8_UNORM;
        static constexpr TinyImageFormat ShadowDepthBufferFormat = TinyImageFormat_D32_SFLOAT;

        static constexpr uint32_t MaxIndirectDrawArgs = 1024;
        static constexpr uint32_t MaxViewportFrameDescriptors = 256;
        static constexpr uint32_t MaxObjectUniforms = 4096;

        static constexpr uint32_t LightClusterWidth = 8;
        static constexpr uint32_t LightClusterHeight = 8;
        static constexpr uint32_t LightClusterLightCount = 128;

        static constexpr uint32_t PointLightCount = 1024;
        static constexpr uint32_t MaxTextureCount = 4096;
        static constexpr uint32_t MaxOpaqueCount = 512;

        struct ViewportData {
        public:
            ViewportData() = default;
            ViewportData(const ViewportData&) = delete;
            ViewportData(ViewportData&& buffer)
                : m_size(buffer.m_size)
                , m_outputBuffer(std::move(buffer.m_outputBuffer))
                , m_depthBuffer(std::move(buffer.m_depthBuffer))
                , m_testBuffer(std::move(buffer.m_testBuffer)) {
            }

            ViewportData& operator=(const ViewportData&) = delete;
            void operator=(ViewportData&& buffer) {
                m_size = buffer.m_size;
                m_outputBuffer = std::move(buffer.m_outputBuffer);
                m_depthBuffer = std::move(buffer.m_depthBuffer);
                m_testBuffer = std::move(buffer.m_testBuffer);
            }

            uint2 m_size = uint2(0, 0);
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_outputBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_testBuffer;
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_depthBuffer;
        };

        struct PointLightData {
            mat4 m_mvp;
            float3 m_lightPos;
            uint m_config;
            float4 m_lightColor;
            float m_radius;
        };

        struct UniformPerFrameData {
            mat4 m_invViewRotation;
            mat4 m_viewMatrix;
            mat4 m_invViewMatrix;
            mat4 m_projectionMatrix;
            mat4 m_viewProjectionMatrix;

            float worldFogStart;
            float worldFogLength;
            float oneMinusFogAlpha;
            float fogFalloffExp;
            float4 fogColor;

            float2 viewTexel;
            float2 viewportSize;
            float afT;
        };

        struct UniformObject {
            float m_dissolveAmount;
            uint m_materialIndex;
            float m_lightLevel;
            float m_illuminationAmount;
            mat4 m_modelMat;
            mat4 m_invModelMat;
            mat4 m_uvMat;
        };

        enum LightType {
            PointLight,
            BoxLight,
        };

        struct PerFrameOption {
            float2 m_size;
            Matrix4 m_viewMat;
            Matrix4 m_projectionMat;
        };

        RendererForwardPlus(cGraphics* apGraphics, cResources* apResources, std::shared_ptr<DebugDraw> debug);

        virtual ~RendererForwardPlus();
        virtual SharedRenderTarget GetOutputImage(uint32_t frameIndex, cViewport& viewport) override;
        virtual void Draw(
            Cmd* cmd,
            const ForgeRenderer::Frame& context,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings,
            bool abSendFrameBufferToPostEffects) override;

        uint32_t resolveObjectId(
            const ForgeRenderer::Frame& frame, cMaterial* apMaterial, iRenderable* apObject, std::optional<cMatrixf> modelMatrix);

    private:
        uint32_t updateFrameDescriptor(const ForgeRenderer::Frame& frame, Cmd* cmd, cWorld* apWorld, const PerFrameOption& options);
        uint32_t resolveMaterialID(const ForgeRenderer::Frame& frame,cMaterial* material);


        std::array<SharedBuffer, MaxViewportFrameDescriptors> m_perFrameBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_objectUniformBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_indirectDrawArgsBuffer;
        SharedBuffer m_opaqueMaterialBuffer;

        // light clusters
        SharedRootSignature m_lightClusterRootSignature;
        SharedPipeline m_PointLightClusterPipeline;
        SharedPipeline m_clearClusterPipeline;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_perFrameLightCluster;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_lightClustersBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_lightClusterCountBuffer;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_pointLightBuffer;
        std::array<std::array<SharedTexture,PointLightCount>, ForgeRenderer::SwapChainLength> m_pointLightAttenutation;

        folly::F14ValueMap<iRenderable*, uint32_t> m_objectDescriptorLookup;

        UniqueViewportData<ViewportData> m_boundViewportData;
        cRenderList m_rendererList;

        uint32_t m_objectIndex = 0;
        uint32_t m_activeFrame = 0;
        uint32_t m_frameIndex = 0;

        SharedRootSignature m_diffuseRootSignature;
        SharedPipeline m_diffusePipeline;
        CommandSignature* m_cmdSignatureVBPass = NULL;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_opaqueFrameSet;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_opaqueConstSet;
        SharedDescriptorSet m_opaqueBatchSet;

        struct SceneMaterial {
        public:
            void* m_material = nullptr;
            uint32_t m_version = 0;
            IndexPoolHandle m_slot;
            resource::MaterialTypes m_resource = std::monostate{};
        };

        std::array<SharedSampler, hpl::resource::MaterialSceneSamplersCount> m_batchSampler;
        std::array<SceneMaterial, cMaterial::MaxMaterialID> m_sceneMaterial;
        IndexPool m_opaqueMaterialPool;
        TextureDescriptorPool m_sceneDescriptorPool;

        SharedTexture m_emptyTexture;
        SharedShader m_diffuseShader;
        SharedShader m_pointlightClusterShader;
        SharedShader m_clearClusterShader;
        SharedTexture m_falloff;

        SharedSampler m_nearestClampSampler;

    };
} // namespace hpl
