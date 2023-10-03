#include "graphics/DebugDraw.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
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
        {
            BufferDesc desc = {};
			desc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            desc.mSize = ImmediateIndexBufferSize;
            desc.pName = "Immediate Vertex Buffer";
			addGPURingBuffer(renderer->Rend(), &desc, &m_vertexBuffer);
	    }
        {
            BufferDesc desc = {};
		    desc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            desc.mSize = ImmediateIndexBufferSize;
            desc.pName = "Immediate Index Buffer";
			addGPURingBuffer(renderer->Rend(), &desc, &m_indexBuffer);
	    }
	    m_colorShader.Load(renderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "debug_line.vert";
            loadDesc.mStages[1].pFileName = "debug_line.frag";
            addShader(renderer->Rend(), &loadDesc, shader);
            return true;
	    });
	    m_color2DShader.Load(renderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "debug_line_2D.vert";
            loadDesc.mStages[1].pFileName = "debug_line.frag";
            addShader(renderer->Rend(), &loadDesc, shader);
            return true;
	    });
	    for(auto& buffer: m_frameBufferUniform) {
            buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
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
        m_rootSignature.Load(renderer->Rend(), [&](RootSignature** sig) {
            std::array shaders = {  m_colorShader.m_handle, m_color2DShader.m_handle};
            RootSignatureDesc rootSignatureDesc = {};
            //const char* pStaticSamplers[] = { "depthSampler" };
            //rootSignatureDesc.mStaticSamplerCount = 1;
            //rootSignatureDesc.ppStaticSamplers = &m_bilinearSampler.m_handle;
            //rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(renderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });
        m_perFrameDescriptorSet.Load(renderer->Rend(), [&](DescriptorSet** set) {
            DescriptorSetDesc setDesc = { m_rootSignature.m_handle,
                                          DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                          DebugDraw::NumberOfPerFrameUniforms };
            addDescriptorSet(renderer->Rend(), &setDesc, set);
            return true;
        });

        for(uint32_t i = 0; i < DebugDraw::NumberOfPerFrameUniforms; i++) {
            std::array<DescriptorData, 1> params = {};
            params[0].pName = "perFrameConstants";
            params[0].ppBuffers = &m_frameBufferUniform[i].m_handle;
            updateDescriptorSet(renderer->Rend(), i, m_perFrameDescriptorSet.m_handle, params.size(), params.data());
        }
        VertexLayout colorVertexLayout = {};
#ifndef USE_THE_FORGE_LEGACY
        vertexLayout.mBindingCount = 1;
        vertexLayout.mBindings[0].mStride = sizeof(float3);
#endif
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
        colorVertexLayout.mAttribs[1].mOffset = sizeof(float) * 3;



        BlendStateDesc blendStateDesc{};
        blendStateDesc.mSrcFactors[0] = BC_ONE;
        blendStateDesc.mDstFactors[0] = BC_ZERO;
        blendStateDesc.mBlendModes[0] = BM_ADD;
#ifdef USE_THE_FORGE_LEGACY
        blendStateDesc.mMasks[0] = RED | GREEN | BLUE;
#else
        blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;
#endif
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
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_rootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_color2DShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &colorVertexLayout;

            m_solidColorPipeline[depth].Load(renderer->Rend(), [&](Pipeline** pipline) {
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
            pipelineSettings.pRootSignature = m_rootSignature.m_handle;
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
            pipelineSettings.pRootSignature = m_rootSignature.m_handle;
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
        std::optional<SharedTexture> image,
        const Vector4& aTint,
        const DebugDrawOptions& options) {
        Vector3 vv1 = (options.m_transform * Vector4(v1, 1.0f)).getXYZ();
        Vector3 vv2 = (options.m_transform * Vector4(v2, 1.0f)).getXYZ();
        Vector3 vv3 = (options.m_transform * Vector4(v3, 1.0f)).getXYZ();
        Vector3 vv4 = (options.m_transform * Vector4(v4, 1.0f)).getXYZ();
        UVQuadRequest request = {};
        request.m_depthTest = options.m_depthTest;
        request.m_v1 = Vector3(vv1.getX(), vv1.getY(), vv1.getZ());
        request.m_v2 = Vector3(vv2.getX(), vv2.getY(), vv2.getZ());
        request.m_v3 = Vector3(vv3.getX(), vv3.getY(), vv3.getZ());
        request.m_v4 = Vector3(vv4.getX(), vv4.getY(), vv4.getZ());
        request.m_uv0 = Vector2(uv0.getX(), uv0.getY());
        request.m_uv1 = Vector2(uv1.getX(), uv1.getY());
        request.m_uvImage = image;
        request.m_color = aTint;
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
    float DebugDraw::BillboardScale(cCamera* apCamera, const Eigen::Vector3f& pos) {
        const auto avViewSpacePosition = cMath::MatrixMul(apCamera->GetViewMatrix(), cVector3f(pos.x(), pos.y(), pos.z()));
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
        std::optional<SharedTexture> image,
        const Vector4& aTint,
        const DebugDrawOptions& options) {
        Matrix4 rotation = Matrix4::identity(); // Eigen::Matrix4f::Identity();
        rotation.setUpper3x3(Matrix3(
            Vector3(m_view[0][0], m_view[0][1], m_view[0][2]),
            Vector3(m_view[1][0], m_view[1][1], m_view[1][2]),
            Vector3(m_view[2][0], m_view[2][1], m_view[2][2])));

        Matrix4 billboard = Matrix4::identity();
        billboard[3][1] = pos.getX();
        billboard[3][2] = pos.getY();
        billboard[3][3] = pos.getZ();
        Matrix4 transform = billboard * rotation;

        Vector2 halfSize = Vector2(size.getX() / 2.0f, size.getY() / 2.0f);
        Vector4 v1 = (transform * Vector4(halfSize.getX(), halfSize.getY(), 0, 1));
        Vector4 v2 = (transform * Vector4(-halfSize.getX(), halfSize.getY(), 0, 1));
        Vector4 v3 = (transform * Vector4(halfSize.getX(), -halfSize.getY(), 0, 1));
        Vector4 v4 = (transform * Vector4(-halfSize.getX(), -halfSize.getY(), 0, 1));
        DrawQuad(v1.getXYZ(), v2.getXYZ(), v3.getXYZ(), v4.getXYZ(), uv0, uv1, image, aTint, options);
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

    void DebugDraw::flush(Cmd* cmd, const cViewport& viewport, const cFrustum& frustum, SharedRenderTarget& targetBuffer, SharedRenderTarget& depthBuffer) {
        m_frameIndex = (m_frameIndex + 1) % NumberOfPerFrameUniforms;
        const cMatrixf mainFrustumView = frustum.GetViewMatrix();
        const cMatrixf mainFrustumProj = frustum.GetProjectionMatrix();
        {
            BufferUpdateDesc updateDesc = { m_frameBufferUniform[m_frameIndex].m_handle, 0, sizeof(FrameUniformBuffer)};
            beginUpdateResource(&updateDesc);
            FrameUniformBuffer  uniformData;
            uniformData.m_viewProjMat = cMath::ToForgeMat(cMath::MatrixMul(mainFrustumProj, mainFrustumView).GetTranspose());
            uniformData.m_viewProj2DMat = Matrix4::frustum(0.0f, viewport.GetSizeU().x, viewport.GetSizeU().y, 0.0f, 0.0f, 1.0f);
            (*reinterpret_cast<FrameUniformBuffer*>(updateDesc.pMappedData)) = uniformData;
            endUpdateResource(&updateDesc, NULL);
        }


        std::array targets = {
            targetBuffer.m_handle,
        };
        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
        cmdBindRenderTargets(
            cmd, targets.size(), targets.data(), depthBuffer.m_handle, &loadActions, nullptr, nullptr, -1, -1);
        cmdSetViewport(cmd, 0.0f, 0.0f, static_cast<float>(targetBuffer.m_handle->mWidth), static_cast<float>(targetBuffer.m_handle->mHeight), 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, static_cast<float>(targetBuffer.m_handle->mWidth), static_cast<float>(targetBuffer.m_handle->mHeight));

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

		    GPURingBufferOffset vb = getGPURingBufferOffset(m_vertexBuffer, vertexBufferSize);
		    GPURingBufferOffset ib = getGPURingBufferOffset(m_indexBuffer, indexBufferSize);

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
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perFrameDescriptorSet.m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);

                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
            }
            endUpdateResource(&vertexUpdateDesc, nullptr);
            endUpdateResource(&indexUpdateDesc, nullptr);
        }

        if(!m_line2DSegments.empty()) {
            struct PositionColorVertex {
                float3 position;
                float4 color;
            };
            const size_t numVertices = m_line2DSegments.size() * 2;
            const size_t vbSize = sizeof(PositionColorVertex) * numVertices;
            const size_t ibSize = sizeof(uint32_t) * numVertices;
			const uint32_t stride = sizeof(PositionColorVertex);
		    GPURingBufferOffset vb = getGPURingBufferOffset(m_vertexBuffer, vbSize);
		    GPURingBufferOffset ib = getGPURingBufferOffset(m_indexBuffer, ibSize);
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
            endUpdateResource(&vertexUpdateDesc, nullptr);
            endUpdateResource(&indexUpdateDesc, nullptr);
            cmdBindPipeline(cmd, m_lineSegmentPipeline2D.m_handle);
            cmdBindDescriptorSet(cmd, m_frameIndex, m_perFrameDescriptorSet.m_handle);
			cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &stride, &vb.mOffset);
			cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);

            cmdDrawIndexed(cmd, numVertices, 0, 0);
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
		    GPURingBufferOffset vb = getGPURingBufferOffset(m_vertexBuffer, vertexBufferSize);
		    GPURingBufferOffset ib = getGPURingBufferOffset(m_indexBuffer, indexBufferSize);
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
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex ;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = PositionColorVertex {
                        float3(it->m_start.getX(), it->m_start.getY(), it->m_start.getZ()),
                        float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
                    };
                    reinterpret_cast<uint16_t*>(indexUpdateDesc.pMappedData)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                    reinterpret_cast<PositionColorVertex*>(vertexUpdateDesc.pMappedData)[vertexBufferOffset + (vertexBufferIndex++)] = PositionColorVertex {
                        float3(it->m_end.getX(), it->m_end.getY(), it->m_end.getZ()),
                        float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
                    };

                    lastIt = it;
                    it++;
                } while (it != m_lineSegments.end() && it->m_depthTest == lastIt->m_depthTest);

                cmdBindPipeline(cmd, m_linePipeline[static_cast<size_t>(lastIt->m_depthTest)].m_handle);
                cmdBindDescriptorSet(cmd, m_frameIndex, m_perFrameDescriptorSet.m_handle);
			    cmdBindVertexBuffer(cmd, 1, &vb.pBuffer, &vertexBufferStride, &vb.mOffset);
			    cmdBindIndexBuffer(cmd, ib.pBuffer, INDEX_TYPE_UINT16, ib.mOffset);
                cmdDrawIndexed(cmd, indexBufferIndex, indexBufferOffset, vertexBufferOffset);

                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;
            };

            endUpdateResource(&vertexUpdateDesc, nullptr);
            endUpdateResource(&indexUpdateDesc, nullptr);
        }

    //    if(!m_lineSegments.empty()) {
    //        cmdBindPipeline(cmd, m_lineSegmentPipeline.m_handle);
    //        std::sort(m_lineSegments.begin(), m_lineSegments.end(), [](const LineSegmentRequest& a, const LineSegmentRequest& b) {
    //            return a.m_depthTest < b.m_depthTest;
    //        });

    //        struct PositionColorVertex {
    //            float3 position;
    //            float4 color;
    //        };
    //        const size_t numVertices = m_line2DSegments.size() * 2;
    //        const size_t vbSize = sizeof(PositionColorVertex) * numVertices;
    //        const size_t ibSize = sizeof(uint32_t) * numVertices;
	//	    GPURingBufferOffset vb = getGPURingBufferOffset(m_vertexBuffer, vbSize);
	//	    GPURingBufferOffset ib = getGPURingBufferOffset(m_indexBuffer, ibSize);
    //        uint32_t indexBufferOffset = 0;
    //        uint32_t vertexBufferOffset = 0;

	//	    BufferUpdateDesc vertexUpdateDesc = { vb.pBuffer, vb.mOffset, vbSize};
	//	    BufferUpdateDesc indexUpdateDesc = { ib.pBuffer, ib.mOffset, ibSize};

	//	    beginUpdateResource(&vertexUpdateDesc);
	//	    beginUpdateResource(&indexUpdateDesc);
	//	    auto it = m_lineSegments.begin();
	//	    auto lastIt = m_lineSegments.begin();
    //        while(it != m_lineSegments.end()) {

    //             do {
    //                reinterpret_cast<uint16_t*>(vertexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
    //                reinterpret_cast<PositionColorVertex*>(indexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
    //                    float3(it->m_start.getX(), it->m_start.getY(), 0.0f),
    //                    float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
    //                };
    //                reinterpret_cast<uint16_t*>(vertexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
    //                reinterpret_cast<PositionColorVertex*>(indexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
    //                    float3(it->m_end.getX(), it->m_end.getY(), 0.0f),
    //                    float4(it->m_color.getX(), it->m_color.getY(), it->m_color.getZ(), it->m_color.getW())
    //                };

    //                lastIt = it;
    //                it++;
    //            } while (it != m_lineSegments.end() && it->m_depthTest == lastIt->m_depthTest);
    //        }

    //        for(auto& segment: m_lineSegments) {
    //            reinterpret_cast<uint16_t*>(vertexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
    //            reinterpret_cast<PositionColorVertex*>(indexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
    //                float3(segment.m_start.getX(), segment.m_start.getY(), 0.0f),
    //                float4(segment.m_color.getX(), segment.m_color.getY(), segment.m_color.getZ(), segment.m_color.getW())
    //            };
    //            reinterpret_cast<uint16_t*>(vertexUpdateDesc.pMappedData)[indexBufferOffset++] = vertexBufferOffset;
    //            reinterpret_cast<PositionColorVertex*>(indexUpdateDesc.pMappedData)[vertexBufferOffset++] = PositionColorVertex {
    //                float3(segment.m_end.getX(), segment.m_end.getY(), 0.0f),
    //                float4(segment.m_color.getX(), segment.m_color.getY(), segment.m_color.getZ(), segment.m_color.getW())
    //            };
    //        }
    //        endUpdateResource(&vertexUpdateDesc, nullptr);
    //        endUpdateResource(&indexUpdateDesc, nullptr);

    //        cmdBindPipeline(cmd, m_lineSegmentPipeline.m_handle);
    //        cmdDrawIndexed(cmd, numVertices, 0, 0);
    //    }

        m_line2DSegments.clear();
        m_uvQuads.clear();
        m_colorQuads.clear();
        m_lineSegments.clear();
        m_colorTriangles.clear();
        //    if (m_line2DSegments.empty() && m_uvQuads.empty() && m_colorQuads.empty() && m_lineSegments.empty() &&
        //    m_colorTriangles.empty()) {
        //        return;
        //    }

        //    cVector2l imageSize = m_target->GetImage()->GetImageSize();
        //    if (!m_colorTriangles.empty()) {
        //        size_t vertexBufferOffset = 0;
        //        size_t indexBufferOffset = 0;

        //        std::sort(m_colorTriangles.begin(), m_colorTriangles.end(), [](const ColorTriRequest& a, const ColorTriRequest& b) {
        //            return a.m_depthTest < b.m_depthTest;
        //        });

        //        const size_t numVertices = m_colorTriangles.size() * 3;

        //        bgfx::TransientVertexBuffer vb;
        //        bgfx::TransientIndexBuffer ib;
        //        bgfx::allocTransientVertexBuffer(&vb, numVertices, layout::PositionColor::layout());
        //        bgfx::allocTransientIndexBuffer(&ib, numVertices);

        //        auto it = m_colorTriangles.begin();
        //        auto lastIt = m_colorTriangles.begin();
        //        while (it != m_colorTriangles.end()) {
        //            size_t vertexBufferIndex = 0;
        //            size_t indexBufferIndex = 0;
        //            do {
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v1.x(), it->m_v1.y(), it->m_v1.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v2.x(), it->m_v2.y(), it->m_v2.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v3.x(), it->m_v3.y(), it->m_v3.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                lastIt = it;
        //                it++;
        //            } while (it != m_colorTriangles.end() && it->m_depthTest == lastIt->m_depthTest);

        //            GraphicsContext::LayoutStream layout;
        //            layout.m_vertexStreams.push_back({ .m_transient = vb,
        //                                               .m_startVertex = static_cast<uint32_t>(vertexBufferOffset),
        //                                               .m_numVertices = static_cast<uint32_t>(vertexBufferIndex) });
        //            layout.m_indexStream = { .m_transient = ib,
        //                                     .m_startIndex = static_cast<uint32_t>(indexBufferOffset),
        //                                     .m_numIndices = static_cast<uint32_t>(indexBufferIndex) };
        //            indexBufferOffset += indexBufferIndex;
        //            vertexBufferOffset += vertexBufferIndex;

        //            GraphicsContext::ShaderProgram shaderProgram;
        //            shaderProgram.m_handle = m_colorProgram;
        //            shaderProgram.m_configuration.m_depthTest = lastIt->m_depthTest;
        //            shaderProgram.m_configuration.m_rgbBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_alphaBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_write = Write::RGB;

        //            GraphicsContext::DrawRequest request = {
        //                layout,
        //                shaderProgram,
        //            };
        //            // m_context->Submit(m_perspectiveView, request);
        //        }
        //    }

        //    if (!m_colorQuads.empty()) {
        //        size_t vertexBufferOffset = 0;
        //        size_t indexBufferOffset = 0;

        //        std::sort(m_colorQuads.begin(), m_colorQuads.end(), [](const ColorQuadRequest& a, const ColorQuadRequest& b) {
        //            return a.m_depthTest < b.m_depthTest;
        //        });

        //        const size_t numVertices = m_colorQuads.size() * 4;
        //        const size_t numIndices = m_colorQuads.size() * 6;

        //        bgfx::TransientVertexBuffer vb;
        //        bgfx::TransientIndexBuffer ib;
        //        bgfx::allocTransientVertexBuffer(&vb, numVertices, layout::PositionColor::layout());
        //        bgfx::allocTransientIndexBuffer(&ib, numIndices);

        //        auto it = m_colorQuads.begin();
        //        auto lastIt = m_colorQuads.begin();
        //        while (it != m_colorQuads.end()) {
        //            size_t vertexBufferIndex = 0;
        //            size_t indexBufferIndex = 0;
        //            do {
        //                // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v1.x(), it->m_v1.y(), it->m_v1.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v2.x(), it->m_v2.y(), it->m_v2.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v3.x(), it->m_v3.y(), it->m_v3.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                // reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_v4.x(), it->m_v4.y(), it->m_v4.z() }, { it->m_color.x(), it->m_color.y(), it->m_color.z(),
        //                    it->m_color.w() }
        //                };

        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 4;
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 3;
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 2;
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex - 1;

        //                lastIt = it;
        //                it++;
        //            } while (it != m_colorQuads.end() && it->m_depthTest == lastIt->m_depthTest);

        //            GraphicsContext::LayoutStream layout;
        //            layout.m_vertexStreams.push_back({ .m_transient = vb,
        //                                               .m_startVertex = static_cast<uint32_t>(vertexBufferOffset),
        //                                               .m_numVertices = static_cast<uint32_t>(vertexBufferIndex) });
        //            layout.m_indexStream = { .m_transient = ib,
        //                                     .m_startIndex = static_cast<uint32_t>(indexBufferOffset),
        //                                     .m_numIndices = static_cast<uint32_t>(indexBufferIndex) };
        //            indexBufferOffset += indexBufferIndex;
        //            vertexBufferOffset += vertexBufferIndex;

        //            GraphicsContext::ShaderProgram shaderProgram;
        //            shaderProgram.m_handle = m_colorProgram;
        //            shaderProgram.m_configuration.m_depthTest = lastIt->m_depthTest;

        //            shaderProgram.m_configuration.m_rgbBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_alphaBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_write = Write::RGB;

        //            GraphicsContext::DrawRequest request = {
        //                layout,
        //                shaderProgram,
        //            };
        //            // m_context->Submit(m_perspectiveView, request);
        //        }
        //    }
        //    if (!m_line2DSegments.empty()) {
        //        cMatrixf projectionMtx(cMatrixf::Identity);
        //        cVector3f vProjMin(0, 0, -1000);
        //        cVector3f vProjMax(imageSize.x, imageSize.y, 1000);
        //        bx::mtxOrtho(
        //            projectionMtx.v,
        //            vProjMin.x,
        //            vProjMax.x,
        //            vProjMax.y,
        //            vProjMin.y,
        //            vProjMin.z,
        //            vProjMax.z,
        //            0.0f,
        //            bgfx::getCaps()->homogeneousDepth);

        //        const size_t numVertices = m_line2DSegments.size() * 2;

        //        bgfx::TransientVertexBuffer vb;
        //        bgfx::TransientIndexBuffer ib;
        //        bgfx::allocTransientVertexBuffer(&vb, numVertices, layout::PositionColor::layout());
        //        bgfx::allocTransientIndexBuffer(&ib, numVertices);

        //        size_t vertexBufferOffset = 0;
        //        size_t indexBufferOffset = 0;
        //        for (auto& segment : m_line2DSegments) {
        //            reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset++] = vertexBufferOffset;
        //            reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset++] = {
        //                { segment.m_start.x(), segment.m_start.y(), 0 },
        //                { segment.m_color.x(), segment.m_color.y(), segment.m_color.z(), segment.m_color.w() }
        //            };
        //            reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset++] = vertexBufferOffset;
        //            reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset++] = {
        //                { segment.m_end.x(), segment.m_end.y(), 0 },
        //                { segment.m_color.x(), segment.m_color.y(), segment.m_color.z(), segment.m_color.w() }
        //            };
        //        }

        //        GraphicsContext::LayoutStream layout;
        //        layout.m_vertexStreams.push_back({ .m_transient = vb });
        //        layout.m_indexStream = { .m_transient = ib };
        //        layout.m_drawType = eVertexBufferDrawType_Line;

        //        GraphicsContext::ShaderProgram shaderProgram;
        //        shaderProgram.m_handle = m_colorProgram;
        //        shaderProgram.m_configuration.m_depthTest = DepthTest::Always;

        //        shaderProgram.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One,
        //        BlendOperand::One); shaderProgram.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add,
        //        BlendOperand::One, BlendOperand::One); shaderProgram.m_configuration.m_write = Write::RGB;

        //        GraphicsContext::DrawRequest request = {
        //            layout,
        //            shaderProgram,
        //        };
        //        // m_context->Submit(m_orthographicView, request);
        //    }
        //    if (!m_lineSegments.empty()) {
        //        std::sort(m_lineSegments.begin(), m_lineSegments.end(), [](const LineSegmentRequest& a, const LineSegmentRequest& b) {
        //            return a.m_depthTest < b.m_depthTest;
        //        });

        //        const size_t numVertices = m_lineSegments.size() * 2;
        //        bgfx::TransientVertexBuffer vb;
        //        bgfx::TransientIndexBuffer ib;
        //        bgfx::allocTransientVertexBuffer(&vb, numVertices, layout::PositionColor::layout());
        //        bgfx::allocTransientIndexBuffer(&ib, numVertices);

        //        auto it = m_lineSegments.begin();
        //        auto lastIt = m_lineSegments.begin();
        //        size_t vertexBufferOffset = 0;
        //        size_t indexBufferOffset = 0;
        //        while (it != m_lineSegments.end()) {
        //            size_t vertexBufferIndex = 0;
        //            size_t indexBufferIndex = 0;
        //            do {
        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_start.x(), it->m_start.y(), it->m_start.z() },
        //                    { it->m_color.x(), it->m_color.y(), it->m_color.z(), it->m_color.w() }
        //                };

        //                reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
        //                reinterpret_cast<hpl::layout::PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)] = {
        //                    { it->m_end.x(), it->m_end.y(), it->m_end.z() },
        //                    { it->m_color.x(), it->m_color.y(), it->m_color.z(), it->m_color.w() }
        //                };

        //                lastIt = it;
        //                it++;
        //            } while (it != m_lineSegments.end() && it->m_depthTest == lastIt->m_depthTest);

        //            GraphicsContext::LayoutStream layout;
        //            layout.m_vertexStreams.push_back({ .m_transient = vb,
        //                                               .m_startVertex = static_cast<uint32_t>(vertexBufferOffset),
        //                                               .m_numVertices = static_cast<uint32_t>(vertexBufferIndex) });
        //            layout.m_indexStream = { .m_transient = ib,
        //                                     .m_startIndex = static_cast<uint32_t>(indexBufferOffset),
        //                                     .m_numIndices = static_cast<uint32_t>(indexBufferIndex) };
        //            indexBufferOffset += indexBufferIndex;
        //            vertexBufferOffset += vertexBufferIndex;

        //            layout.m_drawType = eVertexBufferDrawType_Line;

        //            GraphicsContext::ShaderProgram shaderProgram;
        //            shaderProgram.m_handle = m_colorProgram;
        //            shaderProgram.m_configuration.m_depthTest = lastIt->m_depthTest;
        //            shaderProgram.m_configuration.m_rgbBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_alphaBlendFunc =
        //                CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //            shaderProgram.m_configuration.m_write = Write::RGB;

        //            GraphicsContext::DrawRequest request = {
        //                layout,
        //                shaderProgram,
        //            };
        //        //    m_context->Submit(m_perspectiveView, request);
        //        }
        //    }

        //    if (!m_uvQuads.empty()) {
        //        size_t vertexBufferOffset = 0;
        //        size_t indexBufferOffset = 0;
        //        const size_t numVertices = m_colorQuads.size() * 4;

        //        bgfx::TransientIndexBuffer ib;
        //        bgfx::allocTransientIndexBuffer(&ib, 6);
        //        uint16_t* indexBuffer = reinterpret_cast<uint16_t*>(ib.data);
        //        indexBuffer[0] = 0;
        //        indexBuffer[1] = 1;
        //        indexBuffer[2] = 2;
        //        indexBuffer[3] = 1;
        //        indexBuffer[4] = 2;
        //        indexBuffer[5] = 3;

             //   for (auto& quad : m_uvQuads) {
             //       bgfx::TransientVertexBuffer vb;
             //       bgfx::allocTransientVertexBuffer(&vb, 4, layout::PositionTexCoord0::layout());
             //       auto* vertexBuffer = reinterpret_cast<hpl::layout::PositionTexCoord0*>(vb.data);

             //       vertexBuffer[0] = {
             //           .m_x = quad.m_v1.x(), .m_y = quad.m_v1.y(), .m_z = quad.m_v1.z(), .m_u = quad.m_uv0.x(), .m_v = quad.m_uv0.y()
             //       };
             //       vertexBuffer[1] = {
             //           .m_x = quad.m_v2.x(), .m_y = quad.m_v2.y(), .m_z = quad.m_v2.z(), .m_u = quad.m_uv1.x(), .m_v = quad.m_uv0.y()
             //       };
             //       vertexBuffer[2] = {
             //           .m_x = quad.m_v3.x(), .m_y = quad.m_v3.y(), .m_z = quad.m_v3.z(), .m_u = quad.m_uv0.x(), .m_v = quad.m_uv1.y()
             //       };
             //       vertexBuffer[3] = {
             //           .m_x = quad.m_v4.x(), .m_y = quad.m_v4.y(), .m_z = quad.m_v4.z(), .m_u = quad.m_uv1.x(), .m_v = quad.m_uv1.y()
             //       };

             //       struct {
             //           float m_r;
             //           float m_g;
             //           float m_b;
             //           float m_a;
             //       } color = { quad.m_color.r, quad.m_color.g, quad.m_color.b, quad.m_color.a };

             //       GraphicsContext::ShaderProgram shaderProgram;
             //       shaderProgram.m_handle = m_uvProgram;

             //       shaderProgram.m_configuration.m_rgbBlendFunc =
             //           CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
             //       shaderProgram.m_configuration.m_alphaBlendFunc =
             //           CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
             //       shaderProgram.m_uniforms.push_back({ m_u_color, &color });
             //       shaderProgram.m_textures.push_back({ m_s_diffuseMap, quad.m_uvImage->GetHandle(), 0 });
             //       shaderProgram.m_configuration.m_write = Write::RGB;
             //       shaderProgram.m_configuration.m_depthTest = quad.m_depthTest;

             //       GraphicsContext::LayoutStream layout;
             //       layout.m_vertexStreams.push_back({
             //           .m_transient = vb,
             //       });
             //       layout.m_indexStream = { .m_transient = ib };

             //    //   GraphicsContext::DrawRequest request = {
             //    //       layout,
             //    //       shaderProgram,
             //    //   };
             //       // m_context->Submit(m_perspectiveView, request);
             //   }
            //}
    }
} // namespace hpl
