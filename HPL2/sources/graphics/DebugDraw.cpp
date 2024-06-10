#include "graphics/DebugDraw.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "graphics/GraphicsAllocator.h"
#include "graphics/VertexBuffer.h"

#include "scene/Camera.h"

#include "math/Math.h"
#include "math/MathTypes.h"
#include <cstdint>

namespace hpl {

    namespace details {
       static CompareMode toCompareMode(DebugDraw::DebugDepthTest test) {
            switch(test) {
                case DebugDraw::DebugDepthTest::None:
                    return CMP_NEVER;
                case DebugDraw::DebugDepthTest::Less:
                    return CMP_LESS;
                case DebugDraw::DebugDepthTest::LessEqual:
                    return CMP_LEQUAL;
                case DebugDraw::DebugDepthTest::Equal:
                    return CMP_EQUAL;
                case DebugDraw::DebugDepthTest::GreaterEqual:
                    return CMP_GEQUAL;
                case DebugDraw::DebugDepthTest::Greater:
                    return CMP_GREATER;
                case DebugDraw::DebugDepthTest::NotEqual:
                    return CMP_NOTEQUAL;
                case DebugDraw::DebugDepthTest::Always:
                    return CMP_ALWAYS;
                default: break;
            }
            ASSERT(false && "unhandled case");
            return CMP_ALWAYS;
        }
    }

    DebugDraw::DebugDraw(ForgeRenderer* renderer) {
        m_bilinearSampler.Load(renderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc bilinearClampDesc = { FILTER_NEAREST,
                                              FILTER_NEAREST,
                                              MIPMAP_MODE_NEAREST,
                                              ADDRESS_MODE_CLAMP_TO_EDGE,
                                              ADDRESS_MODE_CLAMP_TO_EDGE,
                                              ADDRESS_MODE_CLAMP_TO_EDGE };
            addSampler(renderer->Rend(), &bilinearClampDesc, sampler);
            return true;
        });
	    m_colorShader.Load(renderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "debug.vert";
            loadDesc.mStages[1].pFileName = "debug.frag";
            addShader(renderer->Rend(), &loadDesc, shader);
            return true;
	    });
	    m_color2DShader.Load(renderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "debug_2D.vert";
            loadDesc.mStages[1].pFileName = "debug.frag";
            addShader(renderer->Rend(), &loadDesc, shader);
            return true;
	    });
	    m_textureShader.Load(renderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "debug_uv.vert";
            loadDesc.mStages[1].pFileName = "debug_uv.frag";
            addShader(renderer->Rend(), &loadDesc, shader);
            return true;
	    });
	    for(auto& buffer: m_viewBufferUniform) {
            buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = 1;
                desc.mDesc.mStructStride = sizeof(FrameUniformBuffer);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
	    }
        m_colorRootSignature.Load(renderer->Rend(), [&](RootSignature** sig) {
            std::array shaders = {  m_colorShader.m_handle, m_color2DShader.m_handle};
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(renderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });
        m_textureRootSignature.Load(renderer->Rend(), [&](RootSignature** sig) {
            std::array shaders = {  m_textureShader.m_handle};
            RootSignatureDesc rootSignatureDesc = {};
            const char* pStaticSamplers[] = { "textureFilter" };
            rootSignatureDesc.mStaticSamplerCount = 1;
            rootSignatureDesc.ppStaticSamplers = &m_bilinearSampler.m_handle;
            rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(renderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });
        m_perColorViewDescriptorSet.Load(renderer->Rend(), [&](DescriptorSet** set) {
            DescriptorSetDesc setDesc = { m_colorRootSignature.m_handle,
                                          DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                          DebugDraw::NumberOfPerFrameUniforms };
            addDescriptorSet(renderer->Rend(), &setDesc, set);
            return true;
        });
        m_perTextureViewDescriptorSet.Load(renderer->Rend(), [&](DescriptorSet** set) {
            DescriptorSetDesc setDesc = { m_textureRootSignature.m_handle,
                                          DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                          DebugDraw::NumberOfPerFrameUniforms };
            addDescriptorSet(renderer->Rend(), &setDesc, set);
            return true;
        });

        for(auto& desc: m_perTextureDrawDescriptorSet) {
            desc.Load(renderer->Rend(), [&](DescriptorSet** set) {
                DescriptorSetDesc setDesc = { m_textureRootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, NumberOfTextureUnits };
                addDescriptorSet(renderer->Rend(), &setDesc, set);
                return true;
            });
        }

        for(uint32_t i = 0; i < DebugDraw::NumberOfPerFrameUniforms; i++) {
            std::array<DescriptorData, 1> params = {};
            params[0].pName = "perFrameConstants";
            params[0].ppBuffers = &m_viewBufferUniform[i].m_handle;
            updateDescriptorSet(renderer->Rend(), i, m_perColorViewDescriptorSet.m_handle, params.size(), params.data());
            updateDescriptorSet(renderer->Rend(), i, m_perTextureViewDescriptorSet.m_handle, params.size(), params.data());
        }
        VertexLayout uvVertexLayout = {};
        uvVertexLayout.mBindingCount = 1;
        uvVertexLayout.mBindings[0].mStride = sizeof(float3) + sizeof(float2) + sizeof(float4);

        uvVertexLayout.mAttribCount = 3;
        uvVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        uvVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        uvVertexLayout.mAttribs[0].mBinding = 0;
        uvVertexLayout.mAttribs[0].mLocation = 0;
        uvVertexLayout.mAttribs[0].mOffset = 0;

        uvVertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        uvVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        uvVertexLayout.mAttribs[1].mBinding = 0;
        uvVertexLayout.mAttribs[1].mLocation = 1;
        uvVertexLayout.mAttribs[1].mOffset = sizeof(float3);

        uvVertexLayout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
        uvVertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        uvVertexLayout.mAttribs[2].mBinding = 0;
        uvVertexLayout.mAttribs[2].mLocation = 2;
        uvVertexLayout.mAttribs[2].mOffset = sizeof(float3) + sizeof(float2);

        VertexLayout colorVertexLayout = {};
        colorVertexLayout.mBindingCount = 1;
        colorVertexLayout.mBindings[0].mStride = sizeof(float3) + sizeof(float4);
        colorVertexLayout.mAttribCount = 2;
        colorVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        colorVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        colorVertexLayout.mAttribs[0].mBinding = 0;
        colorVertexLayout.mAttribs[0].mLocation = 0;
        colorVertexLayout.mAttribs[0].mOffset = 0;

        colorVertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
        colorVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        colorVertexLayout.mAttribs[1].mBinding = 0;
        colorVertexLayout.mAttribs[1].mLocation = 1;
        colorVertexLayout.mAttribs[1].mOffset = sizeof(float3);

        BlendStateDesc blendStateDesc{};
        blendStateDesc.mSrcFactors[0] = BC_ONE;
        blendStateDesc.mDstFactors[0] = BC_ONE;
        blendStateDesc.mBlendModes[0] = BM_ADD;

        blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;
        blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateDesc.mIndependentBlend = false;

        std::array colorFormats = { ColorBufferFormat };
        for(size_t depth = 0; depth < static_cast<size_t>(DebugDepthTest::Count); depth++){

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = details::toCompareMode(static_cast<DebugDraw::DebugDepthTest>(depth));

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mFillMode = FILL_MODE_SOLID;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_colorRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_colorShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &colorVertexLayout;

            m_solidColorPipeline[depth].Load(renderer->Rend(), [&](Pipeline** pipline) {
                addPipeline(renderer->Rend(), &pipelineDesc, pipline);
                return true;
            });
        }
        for(size_t depth = 0; depth < static_cast<size_t>(DebugDepthTest::Count); depth++){

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = details::toCompareMode(static_cast<DebugDraw::DebugDepthTest>(depth));

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mFillMode = FILL_MODE_SOLID;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_textureRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_textureShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &uvVertexLayout;

            m_texturePipeline[depth].Load(renderer->Rend(), [&](Pipeline** pipline) {
                addPipeline(renderer->Rend(), &pipelineDesc, pipline);
                return true;
            });
        }
        {

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = false;
            depthStateDesc.mDepthWrite = false;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mFillMode = FILL_MODE_SOLID;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_textureRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_textureShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &uvVertexLayout;

            m_texturePipelineNoDepth.Load(renderer->Rend(), [&](Pipeline** pipline) {
                addPipeline(renderer->Rend(), &pipelineDesc, pipline);
                return true;
            });
        }
        for(size_t depth = 0; depth < static_cast<size_t>(DebugDepthTest::Count); depth++) {
            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = details::toCompareMode(static_cast<DebugDraw::DebugDepthTest>(depth));

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mFillMode = FILL_MODE_WIREFRAME;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_colorRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_colorShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &colorVertexLayout;

            m_linePipeline[depth].Load(renderer->Rend(), [&](Pipeline** pipline) {
                addPipeline(renderer->Rend(), &pipelineDesc, pipline);
                return true;
            });
        }
        {
            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = CMP_ALWAYS;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mFillMode = FILL_MODE_WIREFRAME;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_colorRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_color2DShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &colorVertexLayout;
            m_lineSegmentPipeline2D.Load(renderer->Rend(), [&](Pipeline** pipline) {
                addPipeline(renderer->Rend(), &pipelineDesc, pipline);
                return true;
            });
        }
    }

    void DebugDraw::Reset() {
    }

    void DebugDraw::DebugDrawBoxMinMax(
        const Vector3& avMin, const Vector3& avMax, const Vector4& color, const DebugDrawOptions& options) {
        DebugDrawLine(Vector3(avMax.getX(), avMax.getY(), avMax.getZ()), Vector3(avMin.getX(), avMax.getY(), avMax.getZ()), color, options);
        DebugDrawLine(Vector3(avMax.getX(), avMax.getY(), avMax.getZ()), Vector3(avMax.getX(), avMin.getY(), avMax.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMax.getY(), avMax.getZ()), Vector3(avMin.getX(), avMin.getY(), avMax.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMin.getY(), avMax.getZ()), Vector3(avMax.getX(), avMin.getY(), avMax.getZ()), color, options);

        // Neg Z Quad
        DebugDrawLine(Vector3(avMax.getX(), avMax.getY(), avMin.getZ()), Vector3(avMin.getX(), avMax.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMax.getX(), avMax.getY(), avMin.getZ()), Vector3(avMax.getX(), avMin.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMax.getY(), avMin.getZ()), Vector3(avMin.getX(), avMin.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMin.getY(), avMin.getZ()), Vector3(avMax.getX(), avMin.getY(), avMin.getZ()), color, options);

        // Lines between
        DebugDrawLine(Vector3(avMax.getX(), avMax.getY(), avMax.getZ()), Vector3(avMax.getX(), avMax.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMax.getY(), avMax.getZ()), Vector3(avMin.getX(), avMax.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMin.getX(), avMin.getY(), avMax.getZ()), Vector3(avMin.getX(), avMin.getY(), avMin.getZ()), color, options);
        DebugDrawLine(Vector3(avMax.getX(), avMin.getY(), avMax.getZ()), Vector3(avMax.getX(), avMin.getY(), avMin.getZ()), color, options);
    }

    void DebugDraw::DrawTri(
        const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector4& color, const DebugDrawOptions& options) {
        const Vector4 vv1 = options.m_transform * Vector4(v1, 1.0f);
        const Vector4 vv2 = options.m_transform * Vector4(v2, 1.0f);
        const Vector4 vv3 = options.m_transform * Vector4(v3, 1.0f);
        ColorTriRequest request;
        request.m_depthTest = options.m_depthTest;
        request.m_v1 = { vv1.getX(), vv1.getY(), vv1.getZ() };
        request.m_v2 = { vv2.getX(), vv2.getY(), vv2.getZ() };
        request.m_v3 = { vv3.getX(), vv3.getY(), vv3.getZ() };
        request.m_color = { color.getX(), color.getY(), color.getZ(), color.getW() };
        m_colorTriangles.push_back(request);
    }

    void DebugDraw::DrawPyramid(
        const Vector3& baseCenter, const Vector3& top, float halfWidth, const Vector4& color, const DebugDrawOptions& options) {
        Vector3 vNormal = top - baseCenter;
        Vector3 vPoint = baseCenter + Vector3(1, 1, 1);
        Vector3 vRight = normalize(cross(vNormal, vPoint));
        Vector3 vForward = normalize(cross(vNormal, vRight));

        Vector3 topRight = baseCenter + (vRight + vForward) * halfWidth;
        Vector3 topLeft = baseCenter + (vRight - vForward) * halfWidth;
        Vector3 bottomLeft = baseCenter + (vRight * (-1) - vForward) * halfWidth;
        Vector3 bottomRight = baseCenter + (vRight * (-1) + vForward) * halfWidth;

        DrawTri(top, topRight, topLeft, color, options);
        DrawTri(top, topLeft, bottomLeft, color, options);
        DrawTri(top, bottomLeft, bottomRight, color, options);
        DrawTri(top, bottomRight, topRight, color, options);
    }

    void DebugDraw::DebugDraw2DLine(const Vector2& start, const Vector2& end, const Vector4& color) {
        Line2DSegmentRequest request;
        request.m_start = { start.getX(), start.getY() };
        request.m_end = { end.getX(), end.getY() };
        request.m_color = { color.getX(), color.getY(), color.getZ(), color.getW() };
        m_line2DSegments.push_back(request);
    }

    void DebugDraw::DebugDraw2DLineQuad(cRect2f rect, const Vector4& color) {
        DebugDraw2DLine(Vector2(rect.x, rect.y), Vector2(rect.x + rect.w, rect.y), color);
        DebugDraw2DLine(Vector2(rect.x + rect.w, rect.y), Vector2(rect.x + rect.w, rect.y + rect.h), color);
        DebugDraw2DLine(Vector2(rect.x + rect.w, rect.y + rect.h), Vector2(rect.x, rect.y + rect.h), color);
        DebugDraw2DLine(Vector2(rect.x, rect.y + rect.h), Vector2(rect.x, rect.y), color);
    }

    void DebugDraw::DebugDrawLine(
        const Vector3& start, const Vector3& end, const Vector4& color, const DebugDrawOptions& options) {
        Vector3 transformStart = (options.m_transform * Vector4(start, 1.0f)).getXYZ();
        Vector3 transformEnd = (options.m_transform * Vector4(end, 1.0f)).getXYZ();
        LineSegmentRequest request = {};
        request.m_depthTest = options.m_depthTest;
        request.m_start = { transformStart.getX(), transformStart.getY(), transformStart.getZ() };
        request.m_end = { transformEnd.getX(), transformEnd.getY(), transformEnd.getZ() };
        request.m_color = { color.getX(), color.getY(), color.getZ(), color.getW() };
        m_lineSegments.push_back(request);
    }

    void DebugDraw::DrawQuad(
        const Vector3& v1,
        const Vector3& v2,
        const Vector3& v3,
        const Vector3& v4,
        const Vector2& uv0,
        const Vector2& uv1,
        SharedTexture image,
        const Vector4& aTint,
        const DebugDrawOptions& options) {
        Vector3 vv1 = (options.m_transform * Vector4(v1, 1.0f)).getXYZ();
        Vector3 vv2 = (options.m_transform * Vector4(v2, 1.0f)).getXYZ();
        Vector3 vv3 = (options.m_transform * Vector4(v3, 1.0f)).getXYZ();
        Vector3 vv4 = (options.m_transform * Vector4(v4, 1.0f)).getXYZ();
        UVQuadRequest request = {};
        request.m_depthTest = options.m_depthTest;
        UVQuadRequest::Quad quad = UVQuadRequest::Quad{};
        quad.m_v1 = vv1;
        quad.m_v2 = vv2;
        quad.m_v3 = vv3;
        quad.m_v4 = vv4;
        request.m_uv0 = Vector2(uv0.getX(), uv0.getY());
        request.m_uv1 = Vector2(uv1.getX(), uv1.getY());
        request.m_uvImage = image;
        request.m_color = aTint;
        request.m_type = quad;
        m_uvQuads.push_back(request);
    }

    void DebugDraw::DrawQuad(
        const Vector3& v1,
        const Vector3& v2,
        const Vector3& v3,
        const Vector3& v4,
        const Vector4& aColor,
        const DebugDrawOptions& options) {
        Vector3 vv1 = (options.m_transform * Vector4(v1, 1.0f)).getXYZ();
        Vector3 vv2 = (options.m_transform * Vector4(v2, 1.0f)).getXYZ();
        Vector3 vv3 = (options.m_transform * Vector4(v3, 1.0f)).getXYZ();
        Vector3 vv4 = (options.m_transform * Vector4(v4, 1.0f)).getXYZ();

        ColorQuadRequest quad = ColorQuadRequest();
        quad.m_depthTest = options.m_depthTest;
        quad.m_v1 = vv1;
        quad.m_v2 = vv2;
        quad.m_v3 = vv3;
        quad.m_v4 = vv4;
        quad.m_color = aColor;
        m_colorQuads.push_back(quad);
    }

    // scale based on distance from camera
    float DebugDraw::BillboardScale(cCamera* apCamera, const Vector3& pos) {
        const auto avViewSpacePosition = cMath::MatrixMul(apCamera->GetViewMatrix(), cVector3f(pos.getX(), pos.getY(), pos.getZ()));
        switch (apCamera->GetProjectionType()) {
        case eProjectionType_Orthographic:
            return apCamera->GetOrthoViewSize().x * 0.25f;
        case eProjectionType_Perspective:
            return cMath::Abs(avViewSpacePosition.z);
        default:
            break;
        }
        ASSERT(false && "invalid projection type");
        return 0.0f;
    }

    void DebugDraw::DrawBillboard(
        const Vector3& pos,
        const Vector2& size,
        const Vector2& uv0,
        const Vector2& uv1,
        SharedTexture image,
        const Vector4& aTint,
        const DebugDrawOptions& options) {

        UVQuadRequest request = {};
        request.m_depthTest = options.m_depthTest;
        UVQuadRequest::Billboard billboard = UVQuadRequest::Billboard {};
        billboard.m_pos = pos;
        billboard.m_transform = options.m_transform;
        billboard.m_size = size;
        request.m_type = billboard;
        request.m_uv0 = uv0;
        request.m_uv1 = uv1;
        request.m_uvImage = image;
        request.m_color = aTint;
        m_uvQuads.push_back(request);
    }

    void DebugDraw::DebugDrawSphere(const Vector3& pos, float radius, const Vector4& color, const DebugDrawOptions& options) {
        DebugDrawSphere(pos, radius, color, color, color, options);
    }

    void DebugDraw::DebugDrawSphere(
        const Vector3& pos, float radius, const Vector4& c1, const Vector4& c2, const Vector4& c3, const DebugDrawOptions& options) {
        constexpr int alSegments = 32;
        constexpr float afAngleStep = k2Pif / static_cast<float>(alSegments);
        // X Circle:
        for (float a = 0; a < k2Pif; a += afAngleStep) {
            DebugDrawLine(
                Vector3(pos.getX(), pos.getY() + sin(a) * radius, pos.getZ() + cos(a) * radius),
                Vector3(pos.getX(), pos.getY() + sin(a + afAngleStep) * radius, pos.getZ() + cos(a + afAngleStep) * radius),
                c1,
                options);
        }

        // Y Circle:
        for (float a = 0; a < k2Pif; a += afAngleStep) {
            DebugDrawLine(
                Vector3(pos.getX() + cos(a) * radius, pos.getY(), pos.getZ() + sin(a) * radius),
                Vector3(pos.getX() + cos(a + afAngleStep) * radius, pos.getY(), pos.getZ() + sin(a + afAngleStep) * radius),
                c2,
                options);
        }

        // Z Circle:
        for (float a = 0; a < k2Pif; a += afAngleStep) {
            DebugDrawLine(
                Vector3(pos.getX() + cos(a) * radius, pos.getY() + sin(a) * radius, pos.getZ()),
                Vector3(pos.getX() + cos(a + afAngleStep) * radius, pos.getY() + sin(a + afAngleStep) * radius, pos.getZ()),
                c3,
                options);
        }
    }

    void DebugDraw::DebugSolidFromVertexBuffer(
        iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options) {
        ///////////////////////////////////////
        // Set up variables
        int lIndexNum = vertexBuffer->GetRequestNumberIndecies();
        if (lIndexNum < 0)
            lIndexNum = vertexBuffer->GetIndexNum();
        unsigned int* pIndexArray = vertexBuffer->GetIndices();

        float* pVertexArray = vertexBuffer->GetFloatArray(eVertexBufferElement_Position);
        int lVertexStride = vertexBuffer->GetElementNum(eVertexBufferElement_Position);

        Vector3 vTriPos[3];

        ///////////////////////////////////////
        // Iterate through each triangle and draw it as 3 lines
        for (int tri = 0; tri < lIndexNum; tri += 3) {
            ////////////////////////
            // Set the vector with positions of the lines
            for (int idx = 0; idx < 3; idx++) {
                int lVtx = pIndexArray[tri + 2 - idx] * lVertexStride;
                vTriPos[idx].setX(pVertexArray[lVtx + 0]);
                vTriPos[idx].setY(pVertexArray[lVtx + 1]);
                vTriPos[idx].setZ(pVertexArray[lVtx + 2]);
            }

            DrawTri(vTriPos[0], vTriPos[1], vTriPos[2], color, options);
        }
    }
    void DebugDraw::DebugWireFrameFromVertexBuffer(
        iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options) {
        ///////////////////////////////////////
        // Set up variables
        int lIndexNum = vertexBuffer->GetRequestNumberIndecies();
        if (lIndexNum < 0)
            lIndexNum = vertexBuffer->GetIndexNum();
        unsigned int* pIndexArray = vertexBuffer->GetIndices();

        float* pVertexArray = vertexBuffer->GetFloatArray(eVertexBufferElement_Position);
        int lVertexStride = vertexBuffer->GetElementNum(eVertexBufferElement_Position);

        Vector3 vTriPos[3];

        ///////////////////////////////////////
        // Iterate through each triangle and draw it as 3 lines
        for (int tri = 0; tri < lIndexNum; tri += 3) {
            ////////////////////////
            // Set the vector with positions of the lines
            for (int idx = 0; idx < 3; idx++) {
                int lVtx = pIndexArray[tri + 2 - idx] * lVertexStride;

                vTriPos[idx].setX(pVertexArray[lVtx + 0]);
                vTriPos[idx].setY(pVertexArray[lVtx + 1]);
                vTriPos[idx].setZ(pVertexArray[lVtx + 2]);
            }

            ////////////////////////
            // Draw the three lines
            for (int i = 0; i < 3; ++i) {
                int lNext = i == 2 ? 0 : i + 1;
                DebugDrawLine(vTriPos[i], vTriPos[lNext], color, options);
            }
        }
    }

    void DebugDraw::flush(const ForgeRenderer::Frame& frame, Cmd* cmd, const cViewport& viewport, const cFrustum& frustum, SharedRenderTarget& targetBuffer, SharedRenderTarget& depthBuffer) {

        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        if (frame.m_currentFrame != m_activeFrame) {
            m_textureLookup.clear();
            m_textureId = 0;
            m_activeFrame = frame.m_currentFrame;
        }


        cmdBeginDebugMarker(cmd, 0, 1, 0, "Debug Flush");

        m_frameIndex = (m_frameIndex + 1) % NumberOfPerFrameUniforms;
        const Matrix4 matMainFrustumView = cMath::ToForgeMatrix4(frustum.GetViewMatrix());
        const Matrix4 matMainFrustumProj = cMath::ToForgeMatrix4(frustum.GetProjectionMatrix());

        Matrix4 correctionMatrix =
            Matrix4(
                Vector4(1.0f, 0, 0, 0),
                Vector4(0, 1.0f, 0, 0),
                Vector4(0, 0, 0.5f, 0),
                Vector4(0, 0, 0.5f, 1.0f)
            );


        const Matrix4 view = cMath::ToForgeMatrix4(frustum.GetViewMatrix());
        {
            BufferUpdateDesc updateDesc = { m_viewBufferUniform[m_frameIndex].m_handle, 0, sizeof(FrameUniformBuffer)};
            beginUpdateResource(&updateDesc);
            FrameUniformBuffer  uniformData;

            uniformData.m_viewProjMat  = ((frustum.GetProjectionType() == eProjectionType_Perspective ? Matrix4::identity() : correctionMatrix) * matMainFrustumProj) * matMainFrustumView ;
            uniformData.m_viewProj2DMat = Matrix4::orthographicLH(0.0f, targetBuffer.m_handle->mWidth, targetBuffer.m_handle->mHeight,0.0f, -1000.0f, 1000.0f);
            (*reinterpret_cast<FrameUniformBuffer*>(updateDesc.pMappedData)) = uniformData;
            endUpdateResource(&updateDesc);
        }

        const bool hasDepthBuffer = depthBuffer.IsValid();

        BindRenderTargetsDesc bindTargetDescs = {0};
        bindTargetDescs.mDepthStencil = { depthBuffer.m_handle, LOAD_ACTION_LOAD };
        bindTargetDescs.mRenderTargets[0] = { targetBuffer.m_handle, LOAD_ACTION_LOAD };
        bindTargetDescs.mRenderTargetCount = 1;
        cmdBindRenderTargets(cmd, &bindTargetDescs);
        cmdSetViewport(cmd, 0.0f, 0.0f, static_cast<float>(targetBuffer.m_handle->mWidth), static_cast<float>(targetBuffer.m_handle->mHeight), 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, static_cast<float>(targetBuffer.m_handle->mWidth), static_cast<float>(targetBuffer.m_handle->mHeight));

        if(!m_colorTriangles.empty()) {
            size_t vertexBufferOffset = 0;
            size_t indexBufferOffset = 0;
            struct PositionColorVertex {
                float3 position;
                float4 color;
            };

            std::sort(m_colorTriangles.begin(), m_colorTriangles.end(), [](const ColorTriRequest& a, const ColorTriRequest& b) {
                return a.m_depthTest < b.m_depthTest;
            });

            const size_t numVertices = m_colorTriangles.size() * 3;
            const size_t numIndices = m_colorTriangles.size() * 3;
            const size_t vertexBufferSize = sizeof(PositionColorVertex) * numVertices;
            const size_t indexBufferSize = sizeof(uint16_t) * numIndices;
			const uint32_t vertexBufferStride = sizeof(PositionColorVertex);

		    GPURingBufferOffset vb = graphicsAllocator->allocTransientVertexBuffer(vertexBufferSize);
		    GPURingBufferOffset ib = graphicsAllocator->allocTransientIndexBuffer(indexBufferSize);

            auto it = m_colorTriangles.begin();
            auto lastIt = m_colorTriangles.begin();
		    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vertexBufferSize};
		    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, indexBufferSize};
		    beginUpdateResource(&vertexUpdateDesc);
		    beginUpdateResource(&indexUpdateDesc);
            while (it != m_colorTriangles.end()) {
                size_t vertexBufferIndex = 0;
                size_t indexBufferIndex = 0;
                do {
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v1.getX(), it->m_v1.getY(), it->m_v1.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v2.getX(), it->m_v2.getY(), it->m_v2.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v3.getX(), it->m_v3.getY(), it->m_v3.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    lastIt = it;
                    it++;
                } while (it != m_colorTriangles.end() && it->m_depthTest == lastIt->m_depthTest);

                cmdBindPipeline(cmd, m_solidColorPipeline[static_cast<size_t>(lastIt->m_depthTest)].m_handle);
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perColorViewDescriptorSet.m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);

                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
            }
            endUpdateResource(&vertexUpdateDesc);
            endUpdateResource(&indexUpdateDesc);
        }


        if (!m_colorQuads.empty()) {
            size_t vertexBufferOffset = 0;
            size_t indexBufferOffset = 0;
            struct PositionColorVertex {
                float3 position;
                float4 color;
            };

            std::sort(m_colorQuads.begin(), m_colorQuads.end(), [](const ColorQuadRequest& a, const ColorQuadRequest& b) {
                return a.m_depthTest < b.m_depthTest;
            });

            const size_t numVertices = m_colorQuads.size() * 4;
            const size_t numIndices = m_colorQuads.size() * 6;
            const size_t vertexBufferSize = sizeof(PositionColorVertex) * numVertices;
            const size_t indexBufferSize = sizeof(uint32_t) * numIndices;
			const uint32_t vertexBufferStride = sizeof(PositionColorVertex);

		    GPURingBufferOffset vb = graphicsAllocator->allocTransientVertexBuffer(vertexBufferSize);
		    GPURingBufferOffset ib = graphicsAllocator->allocTransientIndexBuffer(indexBufferSize);

            auto it = m_colorQuads.begin();
            auto lastIt = m_colorQuads.begin();
		    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vertexBufferSize};
		    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, indexBufferSize};
		    beginUpdateResource(&vertexUpdateDesc);
		    beginUpdateResource(&indexUpdateDesc);
            while (it != m_colorQuads.end()) {
                size_t vertexBufferIndex = 0;
                size_t indexBufferIndex = 0;
                do {
                    // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v1.getX(), it->m_v1.getY(), it->m_v1.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v2.getX(), it->m_v2.getY(), it->m_v2.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v3.getX(), it->m_v3.getY(), it->m_v3.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = {
                        { it->m_v4.getX(), it->m_v4.getY(), it->m_v4.getZ() },
                        { it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW() }
                    };

                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 4;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 1;

                    lastIt = it;
                    it++;
                } while (it != m_colorQuads.end() && it->m_depthTest == lastIt->m_depthTest);

                cmdBindPipeline(cmd, m_solidColorPipeline[static_cast<size_t>(lastIt->m_depthTest)].m_handle);
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perColorViewDescriptorSet.m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);

                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
            }
            endUpdateResource(&vertexUpdateDesc);
            endUpdateResource(&indexUpdateDesc);
        }

        if(!m_line2DSegments.empty()) {
            struct PositionColorVertex {
                float3 position;
                float4 color;
            };
            const size_t numVertices = m_line2DSegments.size() * 2;
            const size_t vbSize = sizeof(PositionColorVertex) * numVertices;
            const size_t ibSize = sizeof(uint16_t) * numVertices;
			const uint32_t stride = sizeof(PositionColorVertex);
		    GPURingBufferOffset vb = graphicsAllocator->allocTransientVertexBuffer(vbSize);
		    GPURingBufferOffset ib = graphicsAllocator->allocTransientIndexBuffer(ibSize);
            uint32_t indexBufferOffset = 0;
            uint32_t vertexBufferOffset = 0;

		    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vbSize};
		    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, ibSize};

		    beginUpdateResource(&vertexUpdateDesc);
		    beginUpdateResource(&indexUpdateDesc);
            for(auto& segment: m_line2DSegments) {
                reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
                reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
                    float3(segment.m_start.getX(), segment.m_start.getY(), 0.0f),
                    float4(segment.m_color.getX(), segment.m_color.getY(), segment.m_color.getZ(), segment.m_color.getW())
                };
                reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
                reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
                    float3(segment.m_end.getX(), segment.m_end.getY(), 0.0f),
                    float4(segment.m_color.getX(), segment.m_color.getY(), segment.m_color.getZ(), segment.m_color.getW())
                };
            }
            endUpdateResource(&vertexUpdateDesc);
            endUpdateResource(&indexUpdateDesc);
            cmdBindPipeline(cmd, m_lineSegmentPipeline2D.m_handle);
            cmdBindDescriptorSet(cmd, m_frameIndex, m_perColorViewDescriptorSet.m_handle);
			cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &stride, &vb.mOffset);
			cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);
            cmdDrawIndexed(cmd, indexBufferOffset, 0, 0);
        }

        if(!m_uvQuads.empty()) {
            struct UVVertex {
                float3 position;
                float2 uv;
                float4 color;
            };
            std::sort(m_uvQuads.begin(), m_uvQuads.end(), [](const UVQuadRequest& a, const UVQuadRequest& b) {
                if(a.m_uvImage.m_handle != b.m_uvImage.m_handle) {
                    return a.m_uvImage.m_handle < b.m_uvImage.m_handle;
                }
                return a.m_depthTest < b.m_depthTest;
            });

            const size_t vertexBufferSize = sizeof(UVVertex) * (m_uvQuads.size() * 4);
            const size_t indexBufferSize = sizeof(uint16_t) * (m_uvQuads.size() * 6);
			const uint32_t vertexBufferStride = sizeof(UVVertex);
		    GPURingBufferOffset vb = graphicsAllocator->allocTransientVertexBuffer(vertexBufferSize);
		    GPURingBufferOffset ib = graphicsAllocator->allocTransientIndexBuffer(indexBufferSize);
            uint32_t indexBufferOffset = 0;
            uint32_t vertexBufferOffset = 0;

		    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vertexBufferSize};
		    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, indexBufferSize};

            beginUpdateResource(&vertexUpdateDesc);
		    beginUpdateResource(&indexUpdateDesc);

            auto it = m_uvQuads.begin();
            auto lastIt = m_uvQuads.begin();
            while (it != m_uvQuads.end()) {
                size_t vertexBufferIndex = 0;
                size_t indexBufferIndex = 0;
                do {

                    std::visit([&](auto& type) {
                        using T = std::decay_t<decltype(type)>;
                        if constexpr (std::is_same_v<T, UVQuadRequest::Quad>) {
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(type.m_v1.getX(), type.m_v1.getY(), type.m_v1.getZ()),
                                float2(it->m_uv0.getX(), it->m_uv0.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(type.m_v2.getX(), type.m_v2.getY(), type.m_v2.getZ()),
                                float2(it->m_uv1.getX(), it->m_uv0.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(type.m_v3.getX(), type.m_v3.getY(), type.m_v3.getZ()),
                                float2(it->m_uv0.getX(), it->m_uv1.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(type.m_v4.getX(), type.m_v4.getY(), type.m_v4.getZ()),
                                float2(it->m_uv1.getX(), it->m_uv1.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                        } else if constexpr (std::is_same_v<T, UVQuadRequest::Billboard>) {
                            Matrix4 rotation = Matrix4::identity(); // Eigen::Matrix4f::Identity();
                            rotation.setUpper3x3(Matrix3(
                                Vector3(view[0][0], view[1][0], view[2][0]),
                                Vector3(view[0][1], view[1][1], view[2][1]),
                                Vector3(view[0][2], view[1][2], view[2][2])));

                            Matrix4 billboard = Matrix4::identity();
                            billboard.setTranslation(type.m_pos);
                            Matrix4 transform = billboard * rotation;

                            Vector2 halfSize = Vector2(type.m_size.getX() / 2.0f, type.m_size.getY() / 2.0f);
                            Vector4 v1 = ((type.m_transform * transform ) * Vector4(halfSize.getX(), halfSize.getY(), 0, 1));
                            Vector4 v2 = ((type.m_transform * transform ) * Vector4(-halfSize.getX(), halfSize.getY(), 0, 1));
                            Vector4 v3 = ((type.m_transform * transform ) * Vector4(halfSize.getX(), -halfSize.getY(), 0, 1));
                            Vector4 v4 = ((type.m_transform * transform ) * Vector4(-halfSize.getX(), -halfSize.getY(), 0, 1));

                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(v1.getX(), v1.getY(), v1.getZ()),
                                float2(it->m_uv0.getX(), it->m_uv0.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(v2.getX(), v2.getY(), v2.getZ()),
                                float2(it->m_uv1.getX(), it->m_uv0.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(v3.getX(), v3.getY(), v3.getZ()),
                                float2(it->m_uv0.getX(), it->m_uv1.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                            reinterpret_cast<UVVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = UVVertex {
                                float3(v4.getX(), v4.getY(), v4.getZ()),
                                float2(it->m_uv1.getX(), it->m_uv1.getY()),
                                float4(it->m_color.getX(),it->m_color.getY(),it->m_color.getZ(),it->m_color.getW())
                            };
                        }
                    }, it->m_type);

                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 4;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 1;

                    lastIt = it;
                    it++;
                } while (it != m_uvQuads.end() &&
                    (it->m_depthTest == lastIt->m_depthTest || !hasDepthBuffer) &&
                    it->m_uvImage.m_handle == lastIt->m_uvImage.m_handle);

                {
                    std::array<DescriptorData, 1> params = {};
                    params[0].pName = "diffuseMap";
                    params[0].ppTextures = &lastIt->m_uvImage.m_handle;
                    updateDescriptorSet(frame.m_renderer->Rend(), m_textureId, m_perTextureDrawDescriptorSet[frame.m_frameIndex].m_handle, params.size(), params.data());
                }
                frame.m_resourcePool->Push(lastIt->m_uvImage);
                if(hasDepthBuffer) {
                    cmdBindPipeline(cmd, m_texturePipeline[static_cast<size_t>(lastIt->m_depthTest)].m_handle);
                } else {
                    cmdBindPipeline(cmd, m_texturePipelineNoDepth.m_handle);
                }
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perTextureViewDescriptorSet.m_handle);
                cmdBindDescriptorSet(cmd, m_textureId, m_perTextureDrawDescriptorSet[frame.m_frameIndex].m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);

                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
                m_textureId++;
            }
            endUpdateResource(&vertexUpdateDesc);
            endUpdateResource(&indexUpdateDesc);

            //auto lookup = m_textureLookup.find(apObject);
        }

        if(!m_lineSegments.empty()) {
            struct PositionColorVertex {
                float3 position;
                float4 color;
            };
            std::sort(m_lineSegments.begin(), m_lineSegments.end(), [](const LineSegmentRequest& a, const LineSegmentRequest& b) {
                return a.m_depthTest < b.m_depthTest;
            });

            const size_t numMaxVertices = m_lineSegments.size() * 2;
            const size_t vertexBufferSize = sizeof(PositionColorVertex) * numMaxVertices;
            const size_t indexBufferSize = sizeof(uint32_t) * numMaxVertices;
			const uint32_t vertexBufferStride = sizeof(PositionColorVertex);

		    GPURingBufferOffset vb = graphicsAllocator->allocTransientVertexBuffer(vertexBufferSize);
		    GPURingBufferOffset ib = graphicsAllocator->allocTransientIndexBuffer(indexBufferSize);
            uint32_t indexBufferOffset = 0;
            uint32_t vertexBufferOffset = 0;

		    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vertexBufferSize};
		    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, indexBufferSize};

            beginUpdateResource(&vertexUpdateDesc);
		    beginUpdateResource(&indexUpdateDesc);

            auto it = m_lineSegments.begin();
            auto lastIt = m_lineSegments.begin();
            while (it != m_lineSegments.end()) {
                size_t vertexBufferIndex = 0;
                size_t indexBufferIndex = 0;
                do {
                    reinterpret_cast<uint32_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex ;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = PositionColorVertex {
                        float3(it->m_start.getX(), it->m_start.getY(), it->m_start.getZ()),
                        float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
                    };
                    reinterpret_cast<uint32_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = PositionColorVertex {
                        float3(it->m_end.getX(), it->m_end.getY(), it->m_end.getZ()),
                        float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
                    };

                    lastIt = it;
                    it++;
                } while (it != m_lineSegments.end() && it->m_depthTest == lastIt->m_depthTest);

                cmdBindPipeline(cmd, m_linePipeline[static_cast<size_t>(lastIt->m_depthTest)].m_handle);
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perColorViewDescriptorSet.m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT32, ib.mOffset);
                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
            };

            endUpdateResource(&vertexUpdateDesc);
            endUpdateResource(&indexUpdateDesc);
        }

        m_line2DSegments.clear();
        m_uvQuads.clear();
        m_colorQuads.clear();
        m_lineSegments.clear();
        m_colorTriangles.clear();
        cmdEndDebugMarker(cmd);
    }
} // namespace hpl
