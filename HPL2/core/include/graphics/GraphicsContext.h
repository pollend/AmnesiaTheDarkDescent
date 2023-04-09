#pragma once

#include "graphics/ShaderUtil.h"
#include "graphics/UniformWrapper.h"
#include "math/Crc32.h"
#include "math/MathTypes.h"

#include <Eigen/Dense>

#include "graphics/Color.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"

#include "resources/StringLiteral.h"
#include "windowing/NativeWindow.h"

#include "engine/QueuedEventLoopHandler.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/inlined_vector.h>
#include <absl/strings/string_view.h>

#include <cstdint>
#include <optional>
#include <queue>
#include <variant>
#include <vector>

#include <bgfx/bgfx.h>

namespace hpl {
    class GraphicsContext;
    class cCamera;

    class GraphicsContext final {
    public:

        struct LayoutStream {
            struct LayoutVertexStream {
                bgfx::TransientVertexBuffer m_transient = { nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
                bgfx::VertexBufferHandle m_handle = BGFX_INVALID_HANDLE;
                bgfx::DynamicVertexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
                uint32_t m_startVertex = 0;
                uint32_t m_numVertices = std::numeric_limits<uint32_t>::max();
            };
            struct LayoutIndexStream {
                bgfx::TransientIndexBuffer m_transient = { nullptr, 0, 0, BGFX_INVALID_HANDLE, false };
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

        void Quad(
            GraphicsContext::LayoutStream& input,
            const cVector3f& pos,
            const cVector2f& size,
            const cVector2f& uv0 = cVector2f(0.0f, 0.0f),
            const cVector2f& uv1 = cVector2f(1.0f, 1.0f));
        void ScreenSpaceQuad(
            GraphicsContext::LayoutStream& input,
            cMatrixf& proj,
            float textureWidth,
            float textureHeight,
            float width = 1.0f,
            float height = 1.0f);
        inline void ConfigureProgram(const GraphicsContext::ShaderProgram& program);
        inline void ConfigureLayoutStream(const GraphicsContext::LayoutStream& layout);

        void Frame();
        inline bgfx::ViewId StartPass(absl::string_view name, const ViewConfiguration& config);

        bool isOriginBottomLeft() const;
        void CopyTextureToFrameBuffer(Image& image, cRect2l dstRect, RenderTarget& target, Write write = Write::RGBA);
        inline void Submit(bgfx::ViewId view, const DrawRequest& request);
        inline void Submit(bgfx::ViewId view, const DrawRequest& request, bgfx::OcclusionQueryHandle query);
        inline void Submit(bgfx::ViewId view, const ComputeRequest& request);

        // eeeh ... not going to bother cleaning up for the moment
        template<StringLiteral VertexShader, StringLiteral FragmentShader>
        bgfx::ProgramHandle resolveProgramCache() {
            constexpr math::Crc32 id = ([]() {
                math::Crc32 crc;
                crc.Update(VertexShader.m_str);
                crc.Update(FragmentShader.m_str);
                return crc;
            })();
           
            auto it = m_programCache.find(id.value());
            if(it != m_programCache.end()) {
                return it->second;
            }
            bgfx::ProgramHandle handle = hpl::loadProgram(VertexShader.m_str, FragmentShader.m_str);
            m_programCache[id.value()] = handle;
            return handle;
        } 

    private:
        absl::flat_hash_map<uint32_t, bgfx::ProgramHandle> m_programCache;

        bgfx::ViewId m_current;
        bgfx::ProgramHandle m_copyProgram = BGFX_INVALID_HANDLE;

        bgfx::ProgramHandle m_uvProgram = BGFX_INVALID_HANDLE; // basic uv shader with tint
        bgfx::ProgramHandle m_colorProgram = BGFX_INVALID_HANDLE;
        bgfx::ProgramHandle m_meshColorProgram = BGFX_INVALID_HANDLE;

        UniformWrapper<StringLiteral("s_diffuseMap"), bgfx::UniformType::Sampler> m_s_diffuseMap;
        UniformWrapper<StringLiteral("u_normalMtx"),  bgfx::UniformType::Mat4> m_u_normalMtx;
        UniformWrapper<StringLiteral("u_color"),      bgfx::UniformType::Vec4> m_u_color;

        window::WindowEvent::QueuedEventHandler m_queuedWindowEvent;
    };

    namespace details {

        inline uint64_t convertBGFXStencil(const StencilTest& stencilTest) {
            if (!IsValidStencilTest(stencilTest)) {
                return BGFX_STENCIL_NONE;
            }
            return BGFX_STENCIL_FUNC_REF(((uint32_t)stencilTest.m_ref)) | BGFX_STENCIL_FUNC_RMASK(((uint32_t)stencilTest.m_mask)) |
                ([&]() -> uint64_t {
                       switch (stencilTest.m_sfail) {
                       case StencilFail::Zero:
                           return BGFX_STENCIL_OP_FAIL_S_ZERO;
                       case StencilFail::Keep:
                           return BGFX_STENCIL_OP_FAIL_S_KEEP;
                       case StencilFail::Replace:
                           return BGFX_STENCIL_OP_FAIL_S_REPLACE;
                       case StencilFail::IncrSat:
                           return BGFX_STENCIL_OP_FAIL_S_INCRSAT;
                       case StencilFail::DecrSat:
                           return BGFX_STENCIL_OP_FAIL_S_DECRSAT;
                       case StencilFail::Invert:
                           return BGFX_STENCIL_OP_FAIL_S_INVERT;
                       case StencilFail::Incr:
                           return BGFX_STENCIL_OP_FAIL_S_INCR;
                       case StencilFail::Decr:
                           return BGFX_STENCIL_OP_FAIL_S_DECR;
                       default:
                           break;
                       }
                       return 0;
                   })() |
                ([&]() -> uint64_t {
                       switch (stencilTest.m_dpfail) {
                       case StencilDepthFail::Zero:
                           return BGFX_STENCIL_OP_FAIL_Z_ZERO;
                       case StencilDepthFail::Keep:
                           return BGFX_STENCIL_OP_FAIL_Z_KEEP;
                       case StencilDepthFail::Replace:
                           return BGFX_STENCIL_OP_FAIL_Z_REPLACE;
                       case StencilDepthFail::IncrSat:
                           return BGFX_STENCIL_OP_FAIL_Z_INCRSAT;
                       case StencilDepthFail::DecrSat:
                           return BGFX_STENCIL_OP_FAIL_Z_DECRSAT;
                       case StencilDepthFail::Invert:
                           return BGFX_STENCIL_OP_FAIL_Z_INVERT;
                       case StencilDepthFail::Incr:
                           return BGFX_STENCIL_OP_FAIL_Z_INCR;
                       case StencilDepthFail::Decr:
                           return BGFX_STENCIL_OP_FAIL_Z_DECR;
                       default:
                           break;
                       }
                       return 0;
                   })() |
                ([&]() -> uint64_t {
                       switch (stencilTest.m_dppass) {
                       case StencilDepthPass::Zero:
                           return BGFX_STENCIL_OP_PASS_Z_ZERO;
                       case StencilDepthPass::Keep:
                           return BGFX_STENCIL_OP_PASS_Z_KEEP;
                       case StencilDepthPass::Replace:
                           return BGFX_STENCIL_OP_PASS_Z_REPLACE;
                       case StencilDepthPass::IncrSat:
                           return BGFX_STENCIL_OP_PASS_Z_INCRSAT;
                       case StencilDepthPass::DecrSat:
                           return BGFX_STENCIL_OP_PASS_Z_DECRSAT;
                       case StencilDepthPass::Invert:
                           return BGFX_STENCIL_OP_PASS_Z_INVERT;
                       case StencilDepthPass::Incr:
                           return BGFX_STENCIL_OP_PASS_Z_INCR;
                       case StencilDepthPass::Decr:
                           return BGFX_STENCIL_OP_PASS_Z_DECR;
                       default:
                           break;
                       }
                       return 0;
                   })() |
                ([&]() -> uint64_t {
                       switch (stencilTest.m_func) {
                       case StencilFunction::Less:
                           return BGFX_STENCIL_TEST_LESS;
                       case StencilFunction::LessEqual:
                           return BGFX_STENCIL_TEST_LEQUAL;
                       case StencilFunction::Equal:
                           return BGFX_STENCIL_TEST_EQUAL;
                       case StencilFunction::GreaterEqual:
                           return BGFX_STENCIL_TEST_GEQUAL;
                       case StencilFunction::Greater:
                           return BGFX_STENCIL_TEST_GREATER;
                       case StencilFunction::NotEqual:
                           return BGFX_STENCIL_TEST_NOTEQUAL;
                       case StencilFunction::Always:
                           return BGFX_STENCIL_TEST_ALWAYS;
                       default:
                           break;
                       }
                       return 0;
                   })();
        }

        inline uint64_t convertBGFXClearOp(const ClearOp& op) {
            uint64_t result = 0;
            if (any(op & ClearOp::Color)) {
                result |= BGFX_CLEAR_COLOR;
            }
            if (any(op & ClearOp::Depth)) {
                result |= BGFX_CLEAR_DEPTH;
            }
            if (any(op & ClearOp::Stencil)) {
                result |= BGFX_CLEAR_STENCIL;
            }
            return result;
        }

        inline uint64_t convertToState(const GraphicsContext::DrawRequest& request) {
            auto& layout = request.m_layout;
            auto& program = request.m_program;

            return (any(program.m_configuration.m_write & Write::Depth) ? BGFX_STATE_WRITE_Z : 0) |
                (any(program.m_configuration.m_write & Write::R) ? BGFX_STATE_WRITE_R : 0) |
                (any(program.m_configuration.m_write & Write::G) ? BGFX_STATE_WRITE_G : 0) |
                (any(program.m_configuration.m_write & Write::B) ? BGFX_STATE_WRITE_B : 0) |
                (any(program.m_configuration.m_write & Write::A) ? BGFX_STATE_WRITE_A : 0) | ([&]() -> uint64_t {
                       switch (program.m_configuration.m_depthTest) {
                       case DepthTest::Always:
                           return BGFX_STATE_DEPTH_TEST_ALWAYS;
                       case DepthTest::Less:
                           return BGFX_STATE_DEPTH_TEST_LESS;
                       case DepthTest::LessEqual:
                           return BGFX_STATE_DEPTH_TEST_LEQUAL;
                       case DepthTest::Equal:
                           return BGFX_STATE_DEPTH_TEST_EQUAL;
                       case DepthTest::GreaterEqual:
                           return BGFX_STATE_DEPTH_TEST_GEQUAL;
                       case DepthTest::Greater:
                           return BGFX_STATE_DEPTH_TEST_GREATER;
                       case DepthTest::NotEqual:
                           return BGFX_STATE_DEPTH_TEST_NOTEQUAL;
                       default:
                           break;
                       }
                       return 0;
                   })() |
                ([&]() -> uint64_t {
                       switch (program.m_configuration.m_cull) {
                       case Cull::Clockwise:
                           return BGFX_STATE_CULL_CW;
                       case Cull::CounterClockwise:
                           return BGFX_STATE_CULL_CCW;
                       default:
                           break;
                       }
                       return 0;
                   })() |
                ([&]() -> uint64_t {
                       switch (layout.m_drawType) {
                       case eVertexBufferDrawType_Tri:
                           break;
                       case eVertexBufferDrawType_TriStrip:
                           return BGFX_STATE_PT_TRISTRIP;
                       case eVertexBufferDrawType_Line:
                           return BGFX_STATE_PT_LINES;
                       case eVertexBufferDrawType_LineStrip:
                           return BGFX_STATE_PT_LINESTRIP;
                       case eVertexBufferDrawType_LineLoop:
                       case eVertexBufferDrawType_TriFan:
                       case eVertexBufferDrawType_Quad:
                       case eVertexBufferDrawType_QuadStrip:
                       default:
                           BX_ASSERT(false, "Unsupported draw type");
                           break;
                       }
                       return 0;
                   })() |
                (program.m_configuration.m_blendAlpha ? BGFX_STATE_BLEND_ALPHA : 0) | ([&] {
                       auto mapToBGFXBlendOperator = [](BlendOperator op) -> uint64_t {
                           switch (op) {
                           case BlendOperator::Add:
                               return BGFX_STATE_BLEND_EQUATION_ADD;
                           case BlendOperator::Subtract:
                               return BGFX_STATE_BLEND_EQUATION_SUB;
                           case BlendOperator::ReverseSubtract:
                               return BGFX_STATE_BLEND_EQUATION_REVSUB;
                           case BlendOperator::Min:
                               return BGFX_STATE_BLEND_EQUATION_MIN;
                           case BlendOperator::Max:
                               return BGFX_STATE_BLEND_EQUATION_MAX;
                           default:
                               break;
                           }
                           return BGFX_STATE_BLEND_EQUATION_ADD;
                       };

                       auto mapToBGFXBlendOperand = [](BlendOperand op) -> uint64_t {
                           switch (op) {
                           case BlendOperand::Zero:
                               return BGFX_STATE_BLEND_ZERO;
                           case BlendOperand::One:
                               return BGFX_STATE_BLEND_ONE;
                           case BlendOperand::SrcColor:
                               return BGFX_STATE_BLEND_SRC_COLOR;
                           case BlendOperand::InvSrcColor:
                               return BGFX_STATE_BLEND_INV_SRC_COLOR;
                           case BlendOperand::SrcAlpha:
                               return BGFX_STATE_BLEND_SRC_ALPHA;
                           case BlendOperand::InvSrcAlpha:
                               return BGFX_STATE_BLEND_INV_SRC_ALPHA;
                           case BlendOperand::DstAlpha:
                               return BGFX_STATE_BLEND_DST_ALPHA;
                           case BlendOperand::InvDestAlpha:
                               return BGFX_STATE_BLEND_INV_DST_ALPHA;
                           case BlendOperand::DstColor:
                               return BGFX_STATE_BLEND_DST_COLOR;
                           case BlendOperand::InvDestColor:
                               return BGFX_STATE_BLEND_INV_DST_COLOR;
                           case BlendOperand::AlphaSat:
                               return BGFX_STATE_BLEND_SRC_ALPHA_SAT;
                           case BlendOperand::BlendFactor:
                               return BGFX_STATE_BLEND_FACTOR;
                           case BlendOperand::BlendInvFactor:
                               return BGFX_STATE_BLEND_INV_FACTOR;
                           default:
                           case BlendOperand::None:
                               break;
                           }
                           return 0;
                       };

                       BlendFunc alphaFunc = program.m_configuration.m_alphaBlendFunc;
                       const auto srcOperandAlpha = mapToBGFXBlendOperand(GetBlendOperandSrc(alphaFunc));
                       const auto destOperandAlpha = mapToBGFXBlendOperand(GetBlendOperandDst(alphaFunc));
                       const auto alphaEquation = mapToBGFXBlendOperator(GetBlendOperator(alphaFunc));

                       BlendFunc rgbFunc = program.m_configuration.m_rgbBlendFunc;
                       const auto srcOperandRgb = mapToBGFXBlendOperand(GetBlendOperandSrc(rgbFunc));
                       const auto destOperandRgb = mapToBGFXBlendOperand(GetBlendOperandDst(rgbFunc));
                       const auto rgbEquation = mapToBGFXBlendOperator(GetBlendOperator(rgbFunc));

                       return BGFX_STATE_BLEND_FUNC_SEPARATE(srcOperandRgb, destOperandRgb, srcOperandAlpha, destOperandAlpha) |
                           BGFX_STATE_BLEND_EQUATION_SEPARATE(rgbEquation, alphaEquation);
                   })();
        }

    } // namespace details

    void GraphicsContext::ConfigureLayoutStream(const GraphicsContext::LayoutStream& layout) {
        uint8_t streamIndex = 0;
        for (auto& vertexStream : layout.m_vertexStreams) {
            if (vertexStream.m_numVertices != std::numeric_limits<uint32_t>::max()) {
                if (bgfx::isValid(vertexStream.m_handle)) {
                    bgfx::setVertexBuffer(streamIndex++, vertexStream.m_handle, vertexStream.m_startVertex, vertexStream.m_numVertices);
                } else if (bgfx::isValid(vertexStream.m_dynamicHandle)) {
                    bgfx::setVertexBuffer(
                        streamIndex++, vertexStream.m_dynamicHandle, vertexStream.m_startVertex, vertexStream.m_numVertices);
                } else if (bgfx::isValid(vertexStream.m_transient.handle)) {
                    bgfx::setVertexBuffer(streamIndex++, &vertexStream.m_transient, vertexStream.m_startVertex, vertexStream.m_numVertices);
                }
            } else {
                if (bgfx::isValid(vertexStream.m_handle)) {
                    bgfx::setVertexBuffer(streamIndex++, vertexStream.m_handle);
                } else if (bgfx::isValid(vertexStream.m_dynamicHandle)) {
                    bgfx::setVertexBuffer(streamIndex++, vertexStream.m_dynamicHandle);
                } else if (bgfx::isValid(vertexStream.m_transient.handle)) {
                    bgfx::setVertexBuffer(streamIndex++, &vertexStream.m_transient);
                }
            }
        }
        if (layout.m_indexStream.m_numIndices != std::numeric_limits<uint32_t>::max()) {
            if (bgfx::isValid(layout.m_indexStream.m_dynamicHandle)) {
                bgfx::setIndexBuffer(
                    layout.m_indexStream.m_dynamicHandle, layout.m_indexStream.m_startIndex, layout.m_indexStream.m_numIndices);
            } else if (bgfx::isValid(layout.m_indexStream.m_handle)) {
                bgfx::setIndexBuffer(layout.m_indexStream.m_handle, layout.m_indexStream.m_startIndex, layout.m_indexStream.m_numIndices);
            } else if (bgfx::isValid(layout.m_indexStream.m_transient.handle)) {
                bgfx::setIndexBuffer(
                    &layout.m_indexStream.m_transient, layout.m_indexStream.m_startIndex, layout.m_indexStream.m_numIndices);
            }
        } else {
            if (bgfx::isValid(layout.m_indexStream.m_dynamicHandle)) {
                bgfx::setIndexBuffer(layout.m_indexStream.m_dynamicHandle);
            } else if (bgfx::isValid(layout.m_indexStream.m_handle)) {
                bgfx::setIndexBuffer(layout.m_indexStream.m_handle);
            } else if (bgfx::isValid(layout.m_indexStream.m_transient.handle)) {
                bgfx::setIndexBuffer(&layout.m_indexStream.m_transient);
            }
        }
    }

    void GraphicsContext::ConfigureProgram(const GraphicsContext::ShaderProgram& program) {
        bgfx::setUniform(m_u_normalMtx, &program.m_normalMtx.v);
        for (auto& uniform : program.m_uniforms) {
            if (bgfx::isValid(uniform.m_uniformHandle)) {
                bgfx::setUniform(uniform.m_uniformHandle, uniform.m_data, uniform.m_num);
            }
        }

        for (auto& texture : program.m_textures) {
            if (bgfx::isValid(texture.m_textureHandle)) {
                bgfx::setTexture(texture.m_stage, texture.m_uniformHandle, texture.m_textureHandle);
            }
        }

        for (auto& uavImage : program.m_uavImage) {
            if (bgfx::isValid(uavImage.m_textureHandle)) {
                bgfx::setImage(uavImage.m_stage, uavImage.m_textureHandle, uavImage.m_mip, uavImage.m_access, uavImage.m_format);
            }
        }
    }

    void GraphicsContext::Submit(bgfx::ViewId view, const DrawRequest& request) {
        auto& layout = request.m_layout;
        auto& program = request.m_program;
        ConfigureLayoutStream(layout);
        ConfigureProgram(program);
        if (IsValidStencilTest(program.m_configuration.m_backStencilTest) ||
            IsValidStencilTest(program.m_configuration.m_frontStencilTest)) {
            bgfx::setStencil(
                details::convertBGFXStencil(program.m_configuration.m_frontStencilTest),
                details::convertBGFXStencil(program.m_configuration.m_backStencilTest));
        }
        bgfx::setTransform(program.m_modelTransform.v);
        bgfx::setState(details::convertToState(request));
        bgfx::submit(view, request.m_program.m_handle);
    }

    void GraphicsContext::Submit(bgfx::ViewId view, const DrawRequest& request, bgfx::OcclusionQueryHandle query) {
        auto& layout = request.m_layout;
        auto& program = request.m_program;

        ConfigureLayoutStream(layout);
        ConfigureProgram(program);

        if (IsValidStencilTest(program.m_configuration.m_backStencilTest) ||
            IsValidStencilTest(program.m_configuration.m_frontStencilTest)) {
            bgfx::setStencil(
                details::convertBGFXStencil(program.m_configuration.m_frontStencilTest),
                details::convertBGFXStencil(program.m_configuration.m_backStencilTest));
        }
        bgfx::setTransform(program.m_modelTransform.v);
        bgfx::setState(details::convertToState(request));
        bgfx::submit(view, request.m_program.m_handle, query);
    }

    void GraphicsContext::Submit(bgfx::ViewId view, const ComputeRequest& request) {
        ConfigureProgram(request.m_program);
        bgfx::dispatch(view, request.m_program.m_handle, request.m_numX, request.m_numY, request.m_numZ);
    }

    bgfx::ViewId GraphicsContext::StartPass(absl::string_view name, const ViewConfiguration& config) {
        bgfx::ViewId view = m_current++;
        bgfx::setViewName(view, name.data());
        if (config.m_clear.has_value()) {
            auto& clear = config.m_clear.value();
            bgfx::setViewClear(view, details::convertBGFXClearOp(clear.m_clearOp), clear.m_rgba, clear.m_depth, clear.m_stencil);
        } else {
            bgfx::setViewClear(view, BGFX_CLEAR_NONE);
        }
        bgfx::setViewTransform(view, config.m_view.v, config.m_projection.v);
        auto& rect = config.m_viewRect;
        bgfx::setViewRect(view, rect.x, rect.y, rect.w, rect.h);
        if (config.m_target.IsValid()) {
            bgfx::setViewFrameBuffer(view, config.m_target.GetHandle());
        } else {
            bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE);
        }
        return view;
    }

} // namespace hpl
