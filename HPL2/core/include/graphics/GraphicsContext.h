#pragma once

#include "math/MathTypes.h"

#include <Eigen/Dense>

#include "graphics/Color.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"

#include "windowing/NativeWindow.h"
#include <bgfx/bgfx.h>


#include <absl/container/inlined_vector.h>
#include <absl/strings/string_view.h>

#include <cstdint>
#include <optional>
#include <queue>
#include <variant>
#include <vector>
#include <optional>

namespace hpl
{
    class GraphicsContext;
    class cCamera;


    class GraphicsContext final
    {
    public:

        struct LayoutStream {
            struct LayoutVertexStream {
                bgfx::TransientVertexBuffer m_transient = {nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE};
                bgfx::VertexBufferHandle m_handle = BGFX_INVALID_HANDLE;
                bgfx::DynamicVertexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
                uint32_t m_startVertex = 0;
                uint32_t m_numVertices = std::numeric_limits<uint32_t>::max();
            };
            struct LayoutIndexStream {
                bgfx::TransientIndexBuffer m_transient = {nullptr, 0, 0, BGFX_INVALID_HANDLE, false};
                bgfx::IndexBufferHandle m_handle = BGFX_INVALID_HANDLE;
                bgfx::DynamicIndexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
                uint32_t m_startIndex = 0;
                uint32_t m_numIndices = std::numeric_limits<uint32_t>::max();
            };
            eVertexBufferDrawType m_drawType = eVertexBufferDrawType_Tri;
            absl::InlinedVector<LayoutVertexStream, 4> m_vertexStreams;
            LayoutIndexStream m_indexStream;
        };

        struct ShaderProgram {
            ShaderProgram();

            bgfx::ProgramHandle m_handle = BGFX_INVALID_HANDLE;
            struct UniformData {
                bgfx::UniformHandle m_uniformHandle = BGFX_INVALID_HANDLE;
                const void* m_data = nullptr;
                uint8_t m_num = 1;
            };
            struct TextureData {
                bgfx::UniformHandle m_uniformHandle = BGFX_INVALID_HANDLE;
                bgfx::TextureHandle m_textureHandle = BGFX_INVALID_HANDLE;
                uint8_t m_stage = 0;
            };
            struct UAVImage {
                bgfx::TextureHandle m_textureHandle = BGFX_INVALID_HANDLE;
                uint8_t m_stage = 0;
                uint8_t m_mip = 0;
                bgfx::Access::Enum m_access = bgfx::Access::Write;
                bgfx::TextureFormat::Enum m_format = bgfx::TextureFormat::Count;
            };

            union {
                struct {
                    StencilTest m_backStencilTest;
                    StencilTest m_frontStencilTest;
                    Write m_write : 5;
                    DepthTest m_depthTest : 3;
                    Cull m_cull : 2;
                    bool m_blendAlpha : 1;
                    BlendFunc m_rgbBlendFunc : 16;
                    BlendFunc m_alphaBlendFunc : 16;
                };
                uint64_t m_state[2] = { 0 };
            } m_configuration;


            cMatrixf m_modelTransform = cMatrixf(cMatrixf::Identity);
            cMatrixf m_normalMtx = cMatrixf(cMatrixf::Identity);

            absl::InlinedVector<TextureData, 10> m_textures;
            absl::InlinedVector<UAVImage, 10> m_uavImage;
            absl::InlinedVector<UniformData, 25> m_uniforms;
        };

        struct ClearRequest {
            uint32_t m_rgba = 0;
            float m_depth = 1.0f;
            uint8_t m_stencil = 0;
            ClearOp m_clearOp = ClearOp::Color | ClearOp::Depth;
        };

        struct DrawClear {
            const RenderTarget& m_target;
            ClearRequest m_clear;

            uint16_t m_x = 0;
            uint16_t m_y = 0;
            uint16_t m_width = 0;
            uint16_t m_height = 0;
        };

        struct DrawRequest {
            const GraphicsContext::LayoutStream& m_layout;
            const ShaderProgram& m_program;
        };

         struct ViewConfiguration {
            const RenderTarget& m_target;

            std::optional<ClearRequest> m_clear;

            cMatrixf m_view = cMatrixf(cMatrixf::Identity);
            cMatrixf m_projection = cMatrixf(cMatrixf::Identity);
            cRect2l m_viewRect = cRect2l(0, 0, 0, 0);
        };

        struct ComputeRequest {
            const ShaderProgram& m_program;

            uint32_t m_numX = 0;
            uint32_t m_numY = 0;
            uint32_t m_numZ = 0;
        };

        GraphicsContext();
        ~GraphicsContext();
        void Init();
        void UpdateScreenSize(uint16_t width, uint16_t height);

        void Quad(GraphicsContext::LayoutStream& input, const cVector3f& pos, const cVector2f& size, const cVector2f& uv0 = cVector2f(0.0f, 0.0f), const cVector2f& uv1 = cVector2f(1.0f, 1.0f));
        void ScreenSpaceQuad(GraphicsContext::LayoutStream& input, cMatrixf& proj, float textureWidth, float textureHeight, float width = 1.0f, float height = 1.0f);
        void ConfigureProgram(const GraphicsContext::ShaderProgram& program);
        
        uint16_t ScreenWidth() const;
        uint16_t ScreenHeight() const;

        void Frame();
        bgfx::ViewId StartPass(absl::string_view name, const ViewConfiguration& config);

        bool isOriginBottomLeft() const;
        void CopyTextureToFrameBuffer(Image& image, cRect2l dstRect, RenderTarget& target, Write write = Write::RGBA);
        void Submit(bgfx::ViewId view, const DrawRequest& request);
        void Submit(bgfx::ViewId view, const DrawRequest& request, bgfx::OcclusionQueryHandle query);
        void Submit(bgfx::ViewId view, const ComputeRequest& request);

    private:
        bgfx::ViewId m_current;
        bgfx::ProgramHandle m_copyProgram = BGFX_INVALID_HANDLE;

        bgfx::ProgramHandle m_uvProgram = BGFX_INVALID_HANDLE; // basic uv shader with tint
        bgfx::ProgramHandle m_colorProgram = BGFX_INVALID_HANDLE;
        bgfx::ProgramHandle m_meshColorProgram = BGFX_INVALID_HANDLE;

        bgfx::UniformHandle m_s_diffuseMap = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle m_u_normalMtx = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle m_u_color = BGFX_INVALID_HANDLE;
        
        window::WindowEvent::Handler m_windowEvent;
        
        friend class ImmediateDrawBatch;
    };


 class ImmediateDrawBatch {
    public:
        struct DebugDrawOptions {
        public:
            DebugDrawOptions() {}
            DepthTest m_depthTest = DepthTest::LessEqual;
            cMatrixf m_transform = cMatrixf(cMatrixf::Identity);
        };

        ImmediateDrawBatch(GraphicsContext& context, RenderTarget& target, const cMatrixf& view, const cMatrixf& projection);
        
        inline GraphicsContext& GetContext() const { return m_context; }

        // takes 3 points and the other 1 is calculated
        void DrawQuad(const cVector3f& v1, const cVector3f& v2, const cVector3f& v3, const cVector2f& uv0,const cVector2f& uv1, hpl::Image* image , const cColor& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawQuad(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const Eigen::Vector3f& v3, const Eigen::Vector2f& uv0, const Eigen::Vector2f& uv1, hpl::Image* image, const Eigen::Vector4f& aTint, const DebugDrawOptions& options = DebugDrawOptions());

        [[deprecated("Use DrawQuad with uv")]]
        void DrawQuad(const cVector3f& v1, const cVector3f& v2, const cVector3f& v3, const cColor& aColor, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawQuad(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const Eigen::Vector3f& v3, const Eigen::Vector4f& color, const DebugDrawOptions& options = DebugDrawOptions());
        

        [[deprecated("Use Drawbillboard with Eigen")]]
        void DrawBillboard(const cVector3f& pos, const cVector2f& size, const cVector2f& uv0, const cVector2f& uv1, hpl::Image* image, const cColor& aTint, const DebugDrawOptions& options = DebugDrawOptions());
        void DrawBillboard(const Eigen::Vector3f& pos, const Eigen::Vector2f& size, const Eigen::Vector2f& uv0, const Eigen::Vector2f& uv1, hpl::Image* image, const Eigen::Vector4f& aTint, const DebugDrawOptions& options = DebugDrawOptions());

         // scale based on distance from camera
        static float BillboardScale(cCamera* apCamera, const Eigen::Vector3f& pos); 

        // draws line 
        void DebugDrawLine(const cVector3f& start, const cVector3f& end, const cColor& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawBoxMinMax(const cVector3f& start, const cVector3f& end, const cColor& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawSphere(const cVector3f& pos, float radius, const cColor& color, const DebugDrawOptions& options = DebugDrawOptions());
        void DebugDrawMesh(const GraphicsContext::LayoutStream& layout, const cColor& color, const DebugDrawOptions& options = DebugDrawOptions());
        void flush();

    private:

        struct ColorQuadRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            Eigen::Vector3f m_v1;
            Eigen::Vector3f m_v2;
            Eigen::Vector3f m_v3;
            Eigen::Vector4f m_color;
        };

        struct UVQuadRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            bool m_billboard;
            Eigen::Vector3f m_v1;
            Eigen::Vector3f m_v2;
            Eigen::Vector3f m_v3;
            Eigen::Vector2f m_uv0;
            Eigen::Vector2f m_uv1;
            hpl::Image* m_uvImage;
            cColor m_color;
        };

        struct DebugMeshRequest {
            GraphicsContext::LayoutStream m_layout;
            DepthTest m_depthTest;
            cMatrixf m_transform;
            cColor m_color;
        };

        struct LineSegmentRequest {
            DepthTest m_depthTest = DepthTest::LessEqual;
            Eigen::Vector3f m_start;
            Eigen::Vector3f m_end;
            Eigen::Vector4f m_color;
        };

        std::vector<UVQuadRequest> m_uvQuads;
        std::vector<ColorQuadRequest> m_colorQuads;
        std::vector<LineSegmentRequest> m_lineSegments;
        std::vector<DebugMeshRequest> m_debugMeshes;
        cMatrixf m_view;
        cMatrixf m_projection;

        RenderTarget& m_target;
        GraphicsContext& m_context;

        friend class GraphicsContext;
    };

} // namespace hpl
