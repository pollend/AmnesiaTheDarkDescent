#include "graphics/RendererForwardPlus.h"

#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/sse/vectormath.hpp"
#include "graphics/Color.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsAllocator.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/IndexPool.h"
#include "scene/Light.h"
#include "scene/RenderableContainer.h"

#include "Common_3/Utilities/Math/MathTypes.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "FixPreprocessor.h"
#include "tinyimageformat_base.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace hpl {

    SharedRenderTarget RendererForwardPlus::GetOutputImage(uint32_t frameIndex, cViewport& viewport) {
        auto sharedData = m_boundViewportData.resolve(viewport);
        if (!sharedData) {
            return SharedRenderTarget();
        }
        return sharedData->m_outputBuffer[frameIndex];
    }

    RendererForwardPlus::RendererForwardPlus(cGraphics* apGraphics, cResources* apResources, std::shared_ptr<DebugDraw> debug)
        : iRenderer("ForwardPlus", apGraphics, apResources),
            m_sceneTexturePool(MaxTextureCount),
            m_sceneTextureDisposable(ForgeRenderer::SwapChainLength),
            m_opaqueMaterialPool(MaxOpaqueCount),
            m_indexResourceDisposable(ForgeRenderer::SwapChainLength) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();

        // shaders
        m_nearestClampSampler.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc samplerDesc = {};
            samplerDesc.mMinFilter = FILTER_NEAREST;
            samplerDesc.mMagFilter = FILTER_NEAREST;
            samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
            samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
            samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
            samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
            addSampler(forgeRenderer->Rend(), &samplerDesc, sampler);
            return true;
        });

        m_emptyTexture.Load([&](Texture** texture) {
            TextureDesc textureDesc = {};
            textureDesc.mArraySize = 1;
            textureDesc.mMipLevels = 1;
            textureDesc.mDepth = 1;
            textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            textureDesc.mWidth = 16;
            textureDesc.mHeight = 16;
            textureDesc.mSampleCount = SAMPLE_COUNT_1;
            textureDesc.mStartState = RESOURCE_STATE_COMMON;
            textureDesc.pName = "empty texture";
            TextureLoadDesc loadDesc = {};
            loadDesc.ppTexture = texture;
            loadDesc.pDesc = &textureDesc;
            addResource(&loadDesc, nullptr);
            return true;
        });

        m_pointlightClusterShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "point_light_clusters.comp";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_clearClusterShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "light_clusters_clear.comp";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_diffuseShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "forward_diffuse.vert";
            loadDesc.mStages[1].pFileName = "forward_diffuse.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });

        // Root signautres
        m_diffuseRootSignature.Load(forgeRenderer->Rend(), [&](RootSignature** sig) {
            ASSERT(m_diffuseShader.IsValid());
            std::array shaders = { m_diffuseShader.m_handle };
            Sampler* vbShadeSceneSamplers[] = { m_nearestClampSampler.m_handle };
            const char* vbShadeSceneSamplersNames[] = { "nearestClampSampler" };
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            rootSignatureDesc.mStaticSamplerCount = std::size(vbShadeSceneSamplersNames);
            rootSignatureDesc.ppStaticSamplers = vbShadeSceneSamplers;
            rootSignatureDesc.ppStaticSamplerNames = vbShadeSceneSamplersNames;
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });
        m_lightClusterRootSignature.Load(forgeRenderer->Rend(), [&](RootSignature** sig) {
            ASSERT(m_pointlightClusterShader.IsValid());
            ASSERT(m_clearClusterShader.IsValid());
            std::array shaders = { m_pointlightClusterShader.m_handle, m_clearClusterShader.m_handle };
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });

        // Indirect Argument signature
        {
            std::array<IndirectArgumentDescriptor, 1> indirectArgs = {};
            {
                IndirectArgumentDescriptor arg{};
                arg.mType = INDIRECT_DRAW_INDEX;
                indirectArgs[0] = arg;
            }
            CommandSignatureDesc vbPassDesc = {
                m_diffuseRootSignature.m_handle,
                indirectArgs.data(),
                indirectArgs.size() };
            vbPassDesc.mPacked = true;
            addIndirectCommandSignature(forgeRenderer->Rend(), &vbPassDesc, &m_cmdSignatureVBPass);
        }

        for(auto& buffer: m_perFrameBuffer) {
            buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mElementCount = 1;
                desc.mDesc.mStructStride = sizeof(UniformPerFrameData);
                desc.mDesc.mSize = sizeof(UniformPerFrameData);
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
        }
        for(size_t swapChainIndex = 0; swapChainIndex < ForgeRenderer::SwapChainLength; swapChainIndex++) {
            m_indirectDrawArgsBuffer[swapChainIndex].Load([&](Buffer** buffer) {
                BufferLoadDesc indirectBufferDesc = {};
                indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
                indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
                indirectBufferDesc.mDesc.mElementCount = MaxIndirectDrawArgs;
                indirectBufferDesc.mDesc.mStructStride = sizeof(IndirectDrawIndexArguments);
                indirectBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT;
                indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
                indirectBufferDesc.mDesc.pName = "Indirect Buffer";
                indirectBufferDesc.ppBuffer = buffer;
                addResource(&indirectBufferDesc, NULL);
                return true;
            });
            m_opaqueMaterialBuffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = MaxOpaqueCount;
                desc.mDesc.mStructStride = sizeof(DiffuseMat);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });

            m_objectUniformBuffer[swapChainIndex].Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = MaxObjectUniforms;
                desc.mDesc.mStructStride = sizeof(UniformObject);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });

            m_lightClustersBuffer[swapChainIndex].Load([&](Buffer** buffer) {
		        BufferLoadDesc bufferDesc = {};
		        bufferDesc.mDesc.mElementCount = LightClusterLightCount * LightClusterWidth  * LightClusterHeight;
		        bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * sizeof(uint32_t);
		        bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
		        bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		        bufferDesc.mDesc.mFirstElement = 0;
		        bufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		        bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		        bufferDesc.mDesc.pName = "Light Cluster";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });

            m_lightClusterCountBuffer[swapChainIndex].Load([&](Buffer** buffer) {
		        BufferLoadDesc bufferDesc = {};
		        bufferDesc.mDesc.mElementCount = LightClusterWidth  * LightClusterHeight;
		        bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * sizeof(uint32_t);
		        bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
		        bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		        bufferDesc.mDesc.mFirstElement = 0;
		        bufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		        bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		        bufferDesc.pData = nullptr;
		        bufferDesc.mDesc.pName = "Light Cluster Count";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });
            m_pointLightBuffer[swapChainIndex].Load([&](Buffer** buffer) {
		        BufferLoadDesc bufferDesc = {};
		        bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
		        bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		        bufferDesc.mDesc.mFirstElement = 0;
		        bufferDesc.mDesc.mStructStride = sizeof(PointLightData);
		        bufferDesc.mDesc.mElementCount = PointLightCount;
		        bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * bufferDesc.mDesc.mStructStride;
		        bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		        bufferDesc.pData = nullptr;
		        bufferDesc.mDesc.pName = "Point Light";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });
        }

        {
            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;

            VertexLayout vertexLayout = {};
#ifndef USE_THE_FORGE_LEGACY
            vertexLayout.mBindingCount = 4;
            vertexLayout.mBindings[0].mStride = sizeof(float3);
            vertexLayout.mBindings[1].mStride = sizeof(float2);
            vertexLayout.mBindings[2].mStride = sizeof(float3);
            vertexLayout.mBindings[3].mStride = sizeof(float3);
#endif
            vertexLayout.mAttribCount = 4;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mOffset = 0;

            vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[2].mBinding = 2;
            vertexLayout.mAttribs[2].mLocation = 2;
            vertexLayout.mAttribs[2].mOffset = 0;

            vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
            vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[3].mBinding = 3;
            vertexLayout.mAttribs[3].mLocation = 3;
            vertexLayout.mAttribs[3].mOffset = 0;
            std::array colorFormats = { ColorBufferFormat , TinyImageFormat_R32G32B32A32_SFLOAT};

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = true;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_diffuseRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_diffuseShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.mSupportIndirectCommandBuffer = true;

            m_diffusePipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });
        }

        m_PointLightClusterPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
            ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
            computePipelineDesc.pShaderProgram = m_pointlightClusterShader.m_handle;
            computePipelineDesc.pRootSignature = m_lightClusterRootSignature.m_handle;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });
        m_clearClusterPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
            ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
            computePipelineDesc.pShaderProgram = m_clearClusterShader.m_handle;
            computePipelineDesc.pRootSignature = m_lightClusterRootSignature.m_handle;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });

        for (size_t swapChainIndex = 0; swapChainIndex < ForgeRenderer::SwapChainLength; swapChainIndex++) {
            m_opaqueFrameSet[swapChainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
                DescriptorSetDesc perFrameDescSet{ m_diffuseRootSignature.m_handle,
                                                   DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                                   MaxViewportFrameDescriptors };
                addDescriptorSet(forgeRenderer->Rend(), &perFrameDescSet, descSet);
                return true;
            });
            m_opaqueConstSet[swapChainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descriptorSet) {
                DescriptorSetDesc constSetDesc{ m_diffuseRootSignature.m_handle,
                                                   DESCRIPTOR_UPDATE_FREQ_NONE,
                                                   1 };
                addDescriptorSet(forgeRenderer->Rend(), &constSetDesc, descriptorSet);
                return true;
            });
            {

                std::array params = {
                    DescriptorData {.pName = "sceneObjects", .ppBuffers = &m_objectUniformBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "opaqueMaterial", .ppBuffers = &m_opaqueMaterialBuffer.m_handle},
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), 0, m_opaqueConstSet[swapChainIndex].m_handle, params.size(), params.data());
            }


            for (size_t i = 0; i < MaxViewportFrameDescriptors; i++) {
                std::array params = {
                    DescriptorData {.pName = "pointLights", .ppBuffers = &m_pointLightBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "lightClustersCount", .ppBuffers = &m_lightClusterCountBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "lightClusters", .ppBuffers = &m_lightClustersBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "perFrameConstants", .ppBuffers = &m_perFrameBuffer[i].m_handle},
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), i, m_opaqueFrameSet[swapChainIndex].m_handle, params.size(), params.data());
            }

            m_perFrameLightCluster[swapChainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
                DescriptorSetDesc perFrameDescSet{ m_lightClusterRootSignature.m_handle,
                                                   DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                                   MaxViewportFrameDescriptors };
                addDescriptorSet(forgeRenderer->Rend(), &perFrameDescSet, descSet);
                return true;
            });
            for (size_t i = 0; i < MaxViewportFrameDescriptors; i++) {
                std::array params = {
                    DescriptorData {.pName = "perFrameConstants", .ppBuffers = &m_perFrameBuffer[i].m_handle},
                    DescriptorData {.pName = "pointLights", .ppBuffers = &m_pointLightBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "lightClustersCount", .ppBuffers = &m_lightClusterCountBuffer[swapChainIndex].m_handle},
                    DescriptorData {.pName = "lightClusters", .ppBuffers = &m_lightClustersBuffer[swapChainIndex].m_handle},
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), i, m_perFrameLightCluster[swapChainIndex].m_handle, params.size(), params.data());
            }
        }
    }
    RendererForwardPlus::~RendererForwardPlus() {
    }

    uint32_t RendererForwardPlus::resolveObjectId(
        const ForgeRenderer::Frame& frame,
        cMaterial* apMaterial,
        iRenderable* apObject,
        std::optional<cMatrixf> modelMatrix) {
        uint32_t id = folly::hash::fnv32_buf(
            reinterpret_cast<const char*>(apObject),
            sizeof(apObject),
            modelMatrix.has_value() ? folly::hash::fnv32_buf(modelMatrix->v, sizeof(modelMatrix->v)) : folly::hash::fnv32_hash_start);

        auto objectLookup = m_objectDescriptorLookup.find(apObject);
        auto& materialDesc = apMaterial->Descriptor();


        auto metaInfo = std::find_if(cMaterial::MaterialMetaTable.begin(), cMaterial::MaterialMetaTable.end(), [&](auto& info) {
            return info.m_id == materialDesc.m_id;
        });

        const bool isFound = objectLookup != m_objectDescriptorLookup.end();
        uint32_t index = isFound ? objectLookup->second : m_objectIndex++;
        if (!isFound) {
            cMatrixf modelMat = modelMatrix.value_or(apObject->GetModelMatrixPtr() ? *apObject->GetModelMatrixPtr() : cMatrixf::Identity);

            UniformObject uniformObjectData = {};
            uniformObjectData.m_dissolveAmount = apObject->GetCoverageAmount();
            uniformObjectData.m_materialIndex = apMaterial->Index();
            uniformObjectData.m_modelMat = cMath::ToForgeMatrix4(modelMat);
            uniformObjectData.m_invModelMat = cMath::ToForgeMatrix4(cMath::MatrixInverse(modelMat));
            uniformObjectData.m_lightLevel = 1.0f;
            uniformObjectData.m_materialIndex = resolveMaterialID(frame, apMaterial);
            if(apMaterial) {
                uniformObjectData.m_uvMat = cMath::ToForgeMatrix4(apMaterial->GetUvMatrix().GetTranspose());
            } else {
                uniformObjectData.m_uvMat = Matrix4::identity();
            }
            BufferUpdateDesc updateDesc = { m_objectUniformBuffer[frame.m_frameIndex].m_handle,
                                            sizeof(UniformObject) * index };
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<UniformObject*>(updateDesc.pMappedData)) = uniformObjectData;
            endUpdateResource(&updateDesc, NULL);

            m_objectDescriptorLookup[apObject] = index;
        }
        return index;
    }
    void RendererForwardPlus::Draw(
        Cmd* cmd,
        const ForgeRenderer::Frame& frame,
        cViewport& viewport,
        float frameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        // keep around for the moment ...
        BeginRendering(frameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects);

        if (frame.m_currentFrame != m_activeFrame) {
            m_objectIndex = 0;
            m_sceneTextureDisposable.reset([&](SharedTexture& texture, IndexPoolHandle& handler) {

            });
            m_objectDescriptorLookup.clear();
            m_activeFrame = frame.m_currentFrame;
        }
        const Matrix4 mainFrustumViewInvMat = inverse(apFrustum->GetViewMat());
        const Matrix4 mainFrustumViewMat = apFrustum->GetViewMat();
        const Matrix4 mainFrustumProjMat = apFrustum->GetProjectionMat();

        auto common = m_boundViewportData.resolve(viewport);
        if (!common || common->m_size != viewport.GetSizeU2()) {
            auto viewportData = std::make_unique<ViewportData>();
            viewportData->m_size = viewport.GetSizeU2();
            for (auto& buffer : viewportData->m_outputBuffer) {
                buffer.Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = optimizedColorClearBlack;
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = viewportData->m_size.x;
                    renderTargetDesc.mHeight = viewportData->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });
            }
            for (auto& buffer : viewportData->m_depthBuffer) {
                buffer.Load(forgeRenderer->Rend(), [&](RenderTarget** handle) {
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = { .depth = 1.0f, .stencil = 0 };
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = viewportData->m_size.x;
                    renderTargetDesc.mHeight = viewportData->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mFormat = DepthBufferFormat;
                    renderTargetDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
                    renderTargetDesc.pName = "Depth RT";
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, handle);
                    return true;
                });
            }
            for (auto& buffer : viewportData->m_testBuffer) {
                buffer.Load(forgeRenderer->Rend(), [&](RenderTarget** handle) {
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = { .depth = 1.0f, .stencil = 0 };
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = viewportData->m_size.x;
                    renderTargetDesc.mHeight = viewportData->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
                    renderTargetDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, handle);
                    return true;
                });
            }
            common = m_boundViewportData.update(viewport, std::move(viewportData));
        }
        uint32_t mainFrameIndex = updateFrameDescriptor(frame, frame.m_cmd, apWorld, {
            .m_size = float2(common->m_size.x, common->m_size.y),
            .m_viewMat = mainFrustumViewMat,
            .m_projectionMat = mainFrustumProjMat
        });

        m_rendererList.BeginAndReset(frameTime, apFrustum);
        struct RenderableContainer {
            iRenderableContainer* m_continer;
            eWorldContainerType m_type;
        };
        std::array worldContainers = {
            RenderableContainer{ apWorld->GetRenderableContainer(eWorldContainerType_Dynamic), eWorldContainerType_Dynamic },
            RenderableContainer{ apWorld->GetRenderableContainer(eWorldContainerType_Static), eWorldContainerType_Static }
        };

        for (auto& container : worldContainers) {
            container.m_continer->UpdateBeforeRendering();
        }
        std::function<void(iRenderableContainerNode * childNode, eWorldContainerType staticGeometry)> walkRenderables;
        walkRenderables = [&](iRenderableContainerNode* childNode, eWorldContainerType containerType) {
            childNode->UpdateBeforeUse();
            for (auto& childNode : childNode->GetChildNodes()) {
                childNode->UpdateBeforeUse();
                eCollision frustumCollision = apFrustum->CollideNode(childNode);
                if (frustumCollision == eCollision_Outside) {
                    continue;
                }
                if (apFrustum->CheckAABBNearPlaneIntersection(childNode->GetMin(), childNode->GetMax())) {
                    cVector3f vViewSpacePos = cMath::MatrixMul(apFrustum->GetViewMatrix(), childNode->GetCenter());
                    childNode->SetViewDistance(vViewSpacePos.z);
                    childNode->SetInsideView(true);
                } else {
                    // Frustum origin is outside of node. Do intersection test.
                    cVector3f vIntersection;
                    cMath::CheckAABBLineIntersection(
                        childNode->GetMin(), childNode->GetMax(), apFrustum->GetOrigin(), childNode->GetCenter(), &vIntersection, NULL);
                    cVector3f vViewSpacePos = cMath::MatrixMul(apFrustum->GetViewMatrix(), vIntersection);
                    childNode->SetViewDistance(vViewSpacePos.z);
                    childNode->SetInsideView(false);
                }
                walkRenderables(childNode, containerType);
            }
            for (auto& pObject : childNode->GetObjects()) {
                if (!rendering::detail::IsObjectIsVisible(pObject, eRenderableFlag_VisibleInNonReflection, {})) {
                    continue;
                }
                cMaterial* pMaterial = pObject->GetMaterial();
                m_rendererList.AddObject(pObject);

                if (pObject && pObject->GetRenderFrameCount() != iRenderer::GetRenderFrameCount()) {
                    pObject->SetRenderFrameCount(iRenderer::GetRenderFrameCount());
                    pObject->UpdateGraphicsForFrame(frameTime);
                }

                if (pMaterial && pMaterial->GetRenderFrameCount() != iRenderer::GetRenderFrameCount()) {
                    pMaterial->SetRenderFrameCount(iRenderer::GetRenderFrameCount());
                    pMaterial->UpdateBeforeRendering(frameTime);
                }
                ////////////////////////////////////////
                // Update per viewport specific and set amtrix point
                // Skip this for non-decal translucent! This is because the water rendering might mess it up otherwise!
                if (pMaterial == NULL || cMaterial::IsTranslucent(pMaterial->Descriptor().m_id) == false ||
                    pMaterial->Descriptor().m_id == MaterialID::Decal) {
                    // skip rendering if the update return false
                    if (pObject->UpdateGraphicsForViewport(apFrustum, frameTime) == false) {
                        return;
                    }

                    pObject->SetModelMatrixPtr(pObject->GetModelMatrix(apFrustum));
                }
                // Only set a matrix used for sorting. Calculate the proper in the trans rendering!
                else {
                    pObject->SetModelMatrixPtr(pObject->GetModelMatrix(NULL));
                }
            }
        };
        for (auto& it : worldContainers) {
            iRenderableContainerNode* pNode = it.m_continer->GetRoot();
            pNode->UpdateBeforeUse(); // Make sure node is updated.
            pNode->SetInsideView(true); // We never want to check root! Assume player is inside.
            walkRenderables(pNode, it.m_type);
        }
        m_rendererList.End(
            eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Translucent | eRenderListCompileFlag_Decal |
            eRenderListCompileFlag_Illumination | eRenderListCompileFlag_FogArea);
        // prepare point light data

        // diffuse
        {
            auto diffuseItems = m_rendererList.GetRenderableItems(eRenderListType_Diffuse);
            BufferUpdateDesc updateDesc = { m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle,
                                            0,
                                            sizeof(IndirectDrawIndexArguments) * diffuseItems.size()};
            beginUpdateResource(&updateDesc);
            for (size_t i = 0; i < diffuseItems.size(); i++) {
                cMaterial* pMaterial = diffuseItems[i]->GetMaterial();
                std::array targets = { eVertexBufferElement_Position,
                                       eVertexBufferElement_Texture0,
                                       eVertexBufferElement_Normal,
                                       eVertexBufferElement_Texture1Tangent };
                DrawPacket packet = diffuseItems[i]->ResolveDrawPacket(frame, targets);
                if (pMaterial == nullptr || packet.m_type == DrawPacket::Unknown) {
                    continue;
                }
                ASSERT(pMaterial->Descriptor().m_id == MaterialID::SolidDiffuse && "Invalid material type");
                ASSERT(packet.m_type == DrawPacket::DrawPacketType::DrawGeometryset);
                ASSERT(packet.m_unified.m_set == GraphicsAllocator::AllocationSet::OpaqueSet);

                auto& args = reinterpret_cast<IndirectDrawIndexArguments*>(updateDesc.pMappedData)[i];
                args.mIndexCount = packet.m_unified.m_numIndices;
                args.mStartIndex = packet.m_unified.m_subAllocation->indexOffset();
                args.mVertexOffset = packet.m_unified.m_subAllocation->vertextOffset();
                args.mStartInstance = resolveObjectId(frame, pMaterial, diffuseItems[i], {}); // for DX12 this won't work
                args.mInstanceCount = 1;
            }
            endUpdateResource(&updateDesc, nullptr);
        }

        {
            auto lights = m_rendererList.GetLights();
            uint32_t numPointLights = std::count_if(lights.begin(), lights.end(), [&](auto& light) {
                return light->GetLightType() == eLightType_Point;
            });
            {
                uint32_t pointLightIndex = 0;
                BufferUpdateDesc pointlightUpdateDesc = { m_pointLightBuffer[frame.m_frameIndex].m_handle,
                                                0,
                                                sizeof(PointLightData) * numPointLights};
                beginUpdateResource(&pointlightUpdateDesc);
                for(auto& light: m_rendererList.GetLights()) {
                    switch(light->GetLightType()) {
                        case eLightType_Point: {
                            m_pointLightAttenutation[frame.m_frameIndex][pointLightIndex] = light->GetFalloffMap()->GetTexture();

                            auto& data = reinterpret_cast<PointLightData*>(pointlightUpdateDesc.pMappedData)[pointLightIndex++];
                            const cColor color = light->GetDiffuseColor();
                            data.m_radius = light->GetRadius();
                            data.m_lightPos = v3ToF3(cMath::ToForgeVec3(light->GetWorldPosition()));
                            data.m_lightColor = float4(color.r, color.g, color.b, color.a);
                            break;
                        }
                        default:
                            break;
                    }
                }
                endUpdateResource(&pointlightUpdateDesc, nullptr);
            }
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

            uint32_t rootConstantIndex = getDescriptorIndexFromName(m_lightClusterRootSignature.m_handle, "uRootConstants");
            cmdBindDescriptorSet(cmd, mainFrameIndex, m_perFrameLightCluster[frame.m_frameIndex].m_handle);
            cmdBindPushConstants(frame.m_cmd, m_lightClusterRootSignature.m_handle, rootConstantIndex, &common->m_size);
            cmdBindPipeline(cmd, m_clearClusterPipeline.m_handle);
            cmdDispatch(cmd, 1, 1, 1);
            {
                std::array barriers = { BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle,
                                                       RESOURCE_STATE_UNORDERED_ACCESS,
                                                       RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(
                    cmd, barriers.size(), barriers.data(), 0, nullptr, 0, nullptr);
            }

            cmdBindDescriptorSet(frame.m_cmd, mainFrameIndex, m_perFrameLightCluster[frame.m_frameIndex].m_handle);
            cmdBindPipeline(cmd, m_PointLightClusterPipeline.m_handle);
            cmdDispatch(cmd, numPointLights, 1, 1);
        }
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array bufferBarrier = {
                BufferBarrier{ m_lightClustersBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE  },
                BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE  },
                BufferBarrier{ m_pointLightBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
            };
            std::array rtBarriers = {
                RenderTargetBarrier{ common->m_outputBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(
                cmd, bufferBarrier.size(), bufferBarrier.data(), 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        {
            auto diffuseItems = m_rendererList.GetRenderableItems(eRenderListType_Diffuse);
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);

            std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                   ShaderSemantic::SEMANTIC_TEXCOORD0,
                                   ShaderSemantic::SEMANTIC_NORMAL,
                                   ShaderSemantic::SEMANTIC_NORMAL };
            cmdBeginDebugMarker(cmd, 0, 1, 0, "Forward Diffuse Pass");
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
            loadActions.mClearColorValues[0] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
            loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
            loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
            std::array targets = {
                common->m_outputBuffer[frame.m_frameIndex].m_handle,
                common->m_testBuffer[frame.m_frameIndex].m_handle
            };
            cmdBindRenderTargets(cmd, targets.size(), targets.data(), common->m_depthBuffer[frame.m_frameIndex].m_handle, &loadActions, NULL, NULL, -1, -1);
            cmdSetViewport(cmd, 0.0f, 0.0f, common->m_size.x, common->m_size.y, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, common->m_size.x, common->m_size.y);

            cmdBindPipeline(cmd, m_diffusePipeline.m_handle);
            opaqueSet.cmdBindGeometrySet(cmd, semantics);
            cmdBindIndexBuffer(cmd, opaqueSet.indexBuffer().m_handle, INDEX_TYPE_UINT32, 0);
            cmdBindDescriptorSet(cmd, 0, m_opaqueConstSet[frame.m_frameIndex].m_handle);
            cmdBindDescriptorSet(cmd, mainFrameIndex, m_opaqueFrameSet[frame.m_frameIndex].m_handle);
		    cmdExecuteIndirect(
			    cmd, m_cmdSignatureVBPass, diffuseItems.size(), m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle, 0, nullptr, 0);
            cmdEndDebugMarker(cmd);
	    }
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array bufferBarrier = {
                BufferBarrier{ m_lightClustersBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS  },
                BufferBarrier{ m_pointLightBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS   },
            };
            std::array rtBarriers = {
                RenderTargetBarrier{ common->m_outputBuffer[frame.m_frameIndex].m_handle, RESOURCE_STATE_RENDER_TARGET ,RESOURCE_STATE_SHADER_RESOURCE},
            };
            cmdResourceBarrier(
                cmd, bufferBarrier.size(), bufferBarrier.data(), 0, NULL, rtBarriers.size(), rtBarriers.data());
        }

    }

    uint32_t RendererForwardPlus::resolveMaterialID(const ForgeRenderer::Frame& frame,cMaterial* material) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        auto& sceneMaterial = m_sceneMaterial[material->Index()];
        uint8_t writeMask = 1 << frame.m_frameIndex;
        if (sceneMaterial.m_material != material ||
            sceneMaterial.m_version != material->Generation() ||
            (sceneMaterial.m_writeMask & writeMask) == 0) {

            auto& descriptor = material->Descriptor();
            sceneMaterial.m_version = material->Generation();
            sceneMaterial.m_material = material;

            if(sceneMaterial.m_writeMask == 0) {
                sceneMaterial.m_materialIndex = IndexPoolHandle(&m_opaqueMaterialPool);
                for(auto& res: sceneMaterial.m_sceneTextures) {
                    if(res.m_handle.isValid()) {
                        m_sceneTextureDisposable.push(std::move(res.m_texture), std::move(res.m_handle));
                    }
                }
                sceneMaterial.m_sceneTextures[eMaterialTexture_Diffuse] = SceneMaterial::SceneTextureEntry {
                     IndexPoolHandle(&m_sceneTexturePool), material->GetImage(eMaterialTexture_Diffuse)->GetTexture()
                };
            }
            sceneMaterial.m_writeMask |= writeMask;
            for(auto& scene: sceneMaterial.m_sceneTextures) {
                if(scene.m_handle.isValid()) {
                    std::array<DescriptorData, 1> params = {
                        DescriptorData {.pName = "sceneTextures", .mCount = 1, .mArrayOffset = scene.m_handle.get(), .ppTextures = &scene.m_texture.m_handle  }
                    };
                    updateDescriptorSet(
                        forgeRenderer->Rend(),
                        0,
                        m_opaqueConstSet[frame.m_frameIndex].m_handle,
                        params.size(),
                        params.data());
                }
            }
            {
                BufferUpdateDesc updateDesc = { m_opaqueMaterialBuffer.m_handle,
                                            sizeof(DiffuseMat) * sceneMaterial.m_materialIndex.get()};
                beginUpdateResource(&updateDesc);
                auto& material = (*reinterpret_cast<DiffuseMat*>(updateDesc.pMappedData));
                material.m_texture[0] = sceneMaterial.m_sceneTextures[eMaterialTexture_Diffuse].m_handle.get();
                endUpdateResource(&updateDesc, NULL);
            }
        }
        return sceneMaterial.m_materialIndex.get();
        //return (sceneMaterial.m_materialIndex.get() << 8) | static_cast<uint8_t>(sceneMaterial.m_id) ;
    }

    uint32_t RendererForwardPlus::updateFrameDescriptor( const ForgeRenderer::Frame& frame, Cmd* cmd, cWorld* apWorld, const PerFrameOption& options) {
        uint32_t index = m_frameIndex;
        BufferUpdateDesc updatePerFrameConstantsDesc = { m_perFrameBuffer[index].m_handle, 0 };
        beginUpdateResource(&updatePerFrameConstantsDesc);
        auto* uniformFrameData = reinterpret_cast<UniformPerFrameData*>(updatePerFrameConstantsDesc.pMappedData);
        uniformFrameData->m_viewMatrix = options.m_viewMat;
        uniformFrameData->m_invViewMatrix = inverse(options.m_viewMat);
        uniformFrameData->m_projectionMatrix = options.m_projectionMat;
        uniformFrameData->m_viewProjectionMatrix = options.m_projectionMat * options.m_viewMat;

        uniformFrameData->worldFogStart = apWorld->GetFogStart();
        uniformFrameData->worldFogLength = apWorld->GetFogEnd() - apWorld->GetFogStart();
        uniformFrameData->oneMinusFogAlpha = 1.0f - apWorld->GetFogColor().a;
        uniformFrameData->fogFalloffExp = apWorld->GetFogFalloffExp();

        uniformFrameData->m_invViewRotation = Matrix4(Matrix4::identity()).setUpper3x3(transpose(options.m_viewMat).getUpper3x3());
        uniformFrameData->viewTexel = float2(1.0f / options.m_size.x, 1.0f / options.m_size.y);
        uniformFrameData->viewportSize = float2(options.m_size.x, options.m_size.y);
        uniformFrameData->afT = GetTimeCount();
        const auto fogColor = apWorld->GetFogColor();
        uniformFrameData->fogColor = float4(fogColor.r, fogColor.g, fogColor.b, fogColor.a);
        endUpdateResource(&updatePerFrameConstantsDesc, NULL);
        m_frameIndex = (m_frameIndex + 1) % MaxViewportFrameDescriptors;
        return index;
    }
} // namespace hpl
