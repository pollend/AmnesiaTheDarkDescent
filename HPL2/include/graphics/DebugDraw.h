#pragma once

#include "graphics/Enum.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/Image.h"

#include "math/MathTypes.h"
#include "math/Matrix.h"

#include "scene/Camera.h"
#include "scene/Viewport.h"
#include "tinyimageformat_base.h"

#include "Common_3/Utilities/RingBuffer.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl {

class DebugDraw final {
        HPL_RTTI_CLASS(iRenderer, "{3D3162A7-C473-4934-BCE0-5220A4FD0A76}")
    public:
        enum class DebugDepthTest: uint8_t {
            None,
            Less,
            LessEqual,
            Equal,
            GreaterEqual,
            Greater,
            NotEqual,
            Always, // Always pass
            Count
        };

        static constexpr uint32_t ImmediateVertexBufferSize = 1 << 10;
        static constexpr uint32_t ImmediateIndexBufferSize = 1 << 10;
        static constexpr uint32_t NumberOfPerFrameUniforms = 32;
        static constexpr uint32_t NumberOfTextureUnits = 512;

        static constexpr TinyImageFormat ColorBufferFormat = TinyImageFormat_R8G8B8A8_UNORM;
        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;

        struct DebugDrawOptions {
        public:
            DebugDrawOptions() {}
            DebugDepthTest m_depthTest = DebugDepthTest::LessEqual;
            Matrix4 m_transform = Matrix4::identity();
        };

        DebugDraw(ForgeRenderer* renderer);
        void Reset();
        void DrawQuad(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& v4, const Vector2& uv0, const Vector2& uv1, SharedTexture image, const Vector4& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawQuad(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& v4, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawTri(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawPyramid(const Vector3& baseCenter, const Vector3& top, float halfWidth, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawBillboard(const Vector3& pos, const Vector2& size, const Vector2& uv0, const Vector2& uv1, SharedTexture image, const Vector4& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDraw2DLine(const Vector2& start, const Vector2& end, const Vector4& color);
        void DebugDraw2DLineQuad(cRect2f rect, const Vector4& color);

        // draws line
        void DebugSolidFromVertexBuffer(iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugWireFrameFromVertexBuffer(iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawBoxMinMax(const Vector3& start, const Vector3& end, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawSphere(const Vector3& pos, float radius, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawSphere(const Vector3& pos, float radius, const Vector4& c1, const Vector4& c2, const Vector4& c3, const DebugDrawOptions& options = DebugDrawOptions());
        void flush(const ForgeRenderer::Frame& frame, Cmd* cmd, const cViewport& viewport, const cFrustum& frustum, SharedRenderTarget& targetBuffer, SharedRenderTarget& depthBuffer);

        // scale based on distance from camera
        static float BillboardScale(cCamera* apCamera, const Vector3& pos);
    private:
        struct ColorQuadRequest {
            DebugDepthTest m_depthTest = DebugDepthTest::LessEqual;
            Vector3 m_v1;
            Vector3 m_v2;
            Vector3 m_v3;
            Vector3 m_v4;
            Vector4 m_color;
        };

        struct ColorTriRequest {
            DebugDepthTest m_depthTest = DebugDepthTest::LessEqual;
            Vector3 m_v1;
            Vector3 m_v2;
            Vector3 m_v3;
            Vector4 m_color;
        };

        struct UVQuadRequest {
            struct Quad {
                Vector3 m_v1;
                Vector3 m_v2;
                Vector3 m_v3;
                Vector3 m_v4;
            };
            struct Billboard {
                Vector3 m_pos;
                Vector2 m_size;
                Matrix4 m_transform;
            };
            DebugDepthTest m_depthTest = DebugDepthTest::LessEqual;
            std::variant<Quad, Billboard> m_type;
            Vector2 m_uv0;
            Vector2 m_uv1;
            SharedTexture m_uvImage;
            Vector4 m_color;
        };

        struct DebugMeshRequest {
            DebugDepthTest m_depthTest;
            Matrix4 m_transform;
            cColor m_color;
        };

        struct LineSegmentRequest {
            DebugDepthTest m_depthTest = DebugDepthTest::LessEqual;
            Vector3 m_start;
            Vector3 m_end;
            Vector4 m_color;
        };

        struct Line2DSegmentRequest {
            Vector2 m_start;
            Vector2 m_end;
            Vector4 m_color;
        };

        struct FrameUniformBuffer {
           mat4 m_viewProjMat;
           mat4 m_viewProj2DMat;
        };

		GPURingBuffer m_vertexBuffer = {};
		GPURingBuffer m_indexBuffer = {};
        std::array<SharedBuffer, NumberOfPerFrameUniforms> m_viewBufferUniform;
        uint32_t m_frameIndex = 0;
        uint32_t m_activeFrame = 0;

        // Orthgraphic projection
        std::vector<Line2DSegmentRequest> m_line2DSegments;
        std::vector<UVQuadRequest> m_uvQuads;
        std::vector<ColorQuadRequest> m_colorQuads;
        std::vector<LineSegmentRequest> m_lineSegments;
        std::vector<ColorTriRequest> m_colorTriangles;

        SharedRootSignature m_colorRootSignature;
        SharedDescriptorSet m_perColorViewDescriptorSet;
        SharedShader m_colorShader;
        SharedShader m_color2DShader;
        SharedPipeline m_lineSegmentPipeline2D;
        std::array<SharedPipeline, static_cast<uint32_t>(DebugDepthTest::Count)> m_solidColorPipeline;
        std::array<SharedPipeline, static_cast<uint32_t>(DebugDepthTest::Count)> m_linePipeline;

        SharedRootSignature m_textureRootSignature;
        SharedDescriptorSet m_perTextureViewDescriptorSet;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_perTextureDrawDescriptorSet;
        SharedShader m_textureShader;
        std::array<SharedPipeline, static_cast<uint32_t>(DebugDepthTest::Count)> m_texturePipeline;
        SharedPipeline m_texturePipelineNoDepth;
        uint32_t m_textureId = 0;

        SharedSampler m_bilinearSampler;
        folly::F14ValueMap<Texture*, uint32_t> m_textureLookup;
    };
}
