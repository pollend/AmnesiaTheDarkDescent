#pragma once

#include <Eigen/Dense>


#include "graphics/Enum.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/Image.h"
#include "scene/Camera.h"

#include "math/MathTypes.h"
#include "math/Matrix.h"

#include "Common_3/Utilities/RingBuffer.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl
{

class ImmediateDrawBatch final {
        HPL_RTTI_CLASS(iRenderer, "{3D3162A7-C473-4934-BCE0-5220A4FD0A76}")
    public:
        static constexpr uint32_t ImmediateVertexBufferSize = 1 << 20;
        static constexpr uint32_t ImmediateIndexBufferSize = 1 << 18;
        static constexpr uint32_t NumberOfPerFrameUniforms = 32;

        struct DebugDrawOptions {
        public:
            DebugDrawOptions() {}
            DepthTest m_depthTest = DepthTest::LessEqual;
            Matrix4 m_transform = Matrix4::identity();
        };


        ImmediateDrawBatch(ForgeRenderer* renderer);
        void Reset();
        void DrawQuad(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& v4, const Vector2& uv0, const Vector2& uv1, std::optional<SharedTexture> image, const Vector4& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawQuad(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& v4, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawTri(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawPyramid(const Vector3& baseCenter, const Vector3& top, float halfWidth, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawBillboard(const Vector3& pos, const Vector2& size, const Vector2& uv0, const Vector2& uv1, std::optional<SharedTexture> image, const Vector4& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDraw2DLine(const Vector2& start, const Vector2& end, const Vector4& color);
        void DebugDraw2DLineQuad(cRect2f rect, const Vector4& color);

        // draws line
        void DebugSolidFromVertexBuffer(iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugWireFrameFromVertexBuffer(iVertexBuffer* vertexBuffer, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawBoxMinMax(const Vector3& start, const Vector3& end, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawSphere(const Vector3& pos, float radius, const Vector4& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawSphere(const Vector3& pos, float radius, const Vector4& c1, const Vector4& c2, const Vector4& c3, const DebugDrawOptions& options = DebugDrawOptions());
        void flush(Cmd* cmd, SharedRenderTarget& targetBuffer, SharedRenderTarget& depthBuffer);

        // scale based on distance from camera
        static float BillboardScale(cCamera* apCamera, const Eigen::Vector3f& pos);
    private:


        struct ColorQuadRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            Vector3 m_v1;
            Vector3 m_v2;
            Vector3 m_v3;
            Vector3 m_v4;
            Vector4 m_color;
        };

        struct ColorTriRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            Vector3 m_v1;
            Vector3 m_v2;
            Vector3 m_v3;
            Vector4 m_color;
        };

        struct UVQuadRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            bool m_billboard;
            Vector3 m_v1;
            Vector3 m_v2;
            Vector3 m_v3;
            Vector3 m_v4;
            Vector2 m_uv0;
            Vector2 m_uv1;
            std::optional<SharedTexture> m_uvImage;
            Vector4 m_color;
        };

        struct DebugMeshRequest {
            DepthTest m_depthTest;
            Matrix4 m_transform;
            cColor m_color;
        };

        struct LineSegmentRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
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
           mat4 viewProjMat;
           mat4 viewProj2DMat;
        };

		GPURingBuffer* m_vertexBuffer = nullptr;
		GPURingBuffer* m_indexBuffer = nullptr;
        std::array<SharedBuffer, NumberOfPerFrameUniforms> m_frameBufferUniform;
        uint32_t m_frameIndex = 0;

        // Orthgraphic projection
        std::vector<Line2DSegmentRequest> m_line2DSegments;
        std::vector<UVQuadRequest> m_uvQuads;
        std::vector<ColorQuadRequest> m_colorQuads;
        std::vector<LineSegmentRequest> m_lineSegments;
        std::vector<ColorTriRequest> m_colorTriangles;

        Matrix3 m_view;
        Matrix3 m_projection;

        SharedRootSignature m_rootSignature;
        SharedPipeline m_lineSegmentPipeline2D;
        SharedPipeline m_lineSegmentPipelineDisableDepth;
        SharedPipeline m_lineSegmentPipeline;
        SharedDescriptorSet m_perFrameDescriptorSet;
        SharedPipeline m_texturePipeline;
        SharedShader m_texturedShader;
        SharedShader m_colorShader;
        SharedShader m_color2DShader;
    };
}
