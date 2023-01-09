#pragma once

#include "absl/container/inlined_vector.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "math/MathTypes.h"
#include <absl/strings/string_view.h>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <optional>
#include <vector>
#include <optional>

namespace hpl
{

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
            absl::InlinedVector<LayoutVertexStream, eVertexBufferElement_LastEnum> m_vertexStreams;
            LayoutIndexStream m_indexStream;
        };

        struct ShaderProgram {
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
                bgfx::Access::Enum m_access;
                bgfx::TextureFormat::Enum m_format;
            };

            union {
                struct {
                    StencilTest m_stencilTest;
                    Write m_write: 5;
                    DepthTest m_depthTest: 3;
                    Cull m_cull: 2;
                    bool m_blendAlpha: 1;
                    BlendFunc m_rgbBlendFunc: 16;
                    BlendFunc m_alphaBlendFunc: 16;
                };
                uint64_t m_state[2] = {0};
            } m_configuration;

            cMatrixf m_modelTransform = cMatrixf(cMatrixf::Identity);
            cMatrixf m_view = cMatrixf(cMatrixf::Identity);
            cMatrixf m_projection = cMatrixf(cMatrixf::Identity);

            absl::InlinedVector<TextureData, 10> m_textures;
            // absl::InlinedVector<TextureData, 10> m_UAVImages;
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
            const RenderTarget& m_target;
            const GraphicsContext::LayoutStream& m_layout;
            const ShaderProgram& m_program;

            std::optional<ClearRequest> m_clear;

            uint16_t m_x = 0;
            uint16_t m_y = 0;
            uint16_t m_width = 0;
            uint16_t m_height = 0;
        };

        GraphicsContext();
        void Init();
        void UpdateScreenSize(uint16_t width, uint16_t height);

        void Quad(GraphicsContext::LayoutStream& input, const cVector3f& pos, const cVector2f& size, const cVector2f& uv0 = cVector2f(0.0f, 0.0f), const cVector2f& uv1 = cVector2f(1.0f, 1.0f));
        void ScreenSpaceQuad(GraphicsContext::LayoutStream& input, cMatrixf& proj, float textureWidth, float textureHeight, float width = 1.0f, float height = 1.0f);
 
        uint16_t ScreenWidth() const;
        uint16_t ScreenHeight() const;

        void Frame();
        bgfx::ViewId StartPass(absl::string_view name);
        bool isOriginBottomLeft() const;
        void CopyTextureToFrameBuffer(bgfx::ViewId view,Image& image, cRect2l dstRect, RenderTarget& target);
        void ClearTarget(bgfx::ViewId view, const DrawClear& request);
        void Submit(bgfx::ViewId view, const DrawRequest& request);
        void Submit(bgfx::ViewId view, const DrawRequest& request, bgfx::OcclusionQueryHandle query);
    private:
        bgfx::ViewId _current;
        bgfx::ProgramHandle m_copyProgram = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle m_s_diffuseMap = BGFX_INVALID_HANDLE;
    };


} // namespace hpl
