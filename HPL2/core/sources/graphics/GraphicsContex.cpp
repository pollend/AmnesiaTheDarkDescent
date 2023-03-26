
#include <graphics/GraphicsContext.h>

#include "engine/Event.h"
#include "engine/Interface.h"

#include "math/Math.h"
#include "math/MathTypes.h"

#include "scene/Camera.h"

#include "graphics/Enum.h"
#include "graphics/ShaderUtil.h"
#include <graphics/GraphicsTypes.h>

#include "bx/math.h"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

#include <bx/debug.h>
#include <algorithm>
#include <variant>
#include <cstddef>
#include <cstdint>
#include <absl/container/inlined_vector.h>
#include <absl/strings/string_view.h>

#include "graphics/Layouts.h"

namespace hpl {

    GraphicsContext::ShaderProgram::ShaderProgram() {
        m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
        m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
    }

    uint64_t convertBGFXStencil(const StencilTest& stencilTest) {
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

    uint64_t convertBGFXClearOp(const ClearOp& op) {
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

    uint64_t convertToState(const GraphicsContext::DrawRequest& request) {
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

    void ConfigureLayoutStream(const GraphicsContext::LayoutStream& layout) {
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

    GraphicsContext::GraphicsContext()
        : m_current(0) {

        m_windowEvent = window::WindowEvent::Handler([&](window::WindowEventPayload& payload) {
            switch(payload.m_type) {
				case hpl::window::WindowEventType::ResizeWindowEvent: {
                    bgfx::reset(payload.payload.m_resizeWindow.m_width, payload.payload.m_resizeWindow.m_height); 
                    break;
                }
                default:
                    break;
            }
        }, ConnectionType::QueueConnection);
        if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			window->ConnectWindowEventHandler(m_windowEvent);
		}

    }

    GraphicsContext::~GraphicsContext() {
        for(auto& it: m_programCache) {
            bgfx::destroy(it.second);
        }
    }

    void GraphicsContext::UpdateScreenSize(uint16_t width, uint16_t height) {
    }

    void GraphicsContext::Init() {

        m_copyProgram = resolveProgramCache<StringLiteral("vs_post_effect"), StringLiteral("fs_post_effect_copy")>();
        m_colorProgram = resolveProgramCache<StringLiteral("vs_color"), StringLiteral("fs_color")>();
        m_uvProgram = resolveProgramCache<StringLiteral("vs_basic_uv"), StringLiteral("fs_basic_uv")>();
        m_meshColorProgram = resolveProgramCache<StringLiteral("vs_color"), StringLiteral("fs_color_1")>();
    }

    void GraphicsContext::Frame() {
        m_current = 0;
        m_windowEvent.Process();
        bgfx::frame();
    }

    void GraphicsContext::Submit(bgfx::ViewId view, const DrawRequest& request) {
        auto& layout = request.m_layout;
        auto& program = request.m_program;
        ConfigureLayoutStream(layout);
        ConfigureProgram(program);
        if (IsValidStencilTest(program.m_configuration.m_backStencilTest) ||
            IsValidStencilTest(program.m_configuration.m_frontStencilTest)) {
            bgfx::setStencil(
                convertBGFXStencil(program.m_configuration.m_frontStencilTest),
                convertBGFXStencil(program.m_configuration.m_backStencilTest));
        }
        bgfx::setTransform(program.m_modelTransform.v);
        bgfx::setState(convertToState(request));
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
                convertBGFXStencil(program.m_configuration.m_frontStencilTest),
                convertBGFXStencil(program.m_configuration.m_backStencilTest));
        }
        bgfx::setTransform(program.m_modelTransform.v);
        bgfx::setState(convertToState(request));
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
            bgfx::setViewClear(view, convertBGFXClearOp(clear.m_clearOp), clear.m_rgba, clear.m_depth, clear.m_stencil);
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

    bool GraphicsContext::isOriginBottomLeft() const {
        BX_ASSERT(bgfx::getCaps(), "GraphicsContext::Init() must be called before isOriginBottomLeft()");
        return bgfx::getCaps()->originBottomLeft;
    }

    void GraphicsContext::CopyTextureToFrameBuffer(Image& image, cRect2l dstRect, RenderTarget& target, Write write) {
        GraphicsContext::LayoutStream layout;
        cMatrixf projMtx;
        ScreenSpaceQuad(layout, projMtx, dstRect.w, dstRect.h);

        GraphicsContext::ViewConfiguration viewConfig{ target };
        viewConfig.m_viewRect = dstRect;
        viewConfig.m_projection = projMtx;
        auto view = StartPass("Copy To Target", viewConfig);

        GraphicsContext::ShaderProgram program;
        program.m_handle = m_copyProgram;
        program.m_configuration.m_write = write;
        // program.m_projection = projMtx;
        program.m_textures.push_back({ m_s_diffuseMap, image.GetHandle(), 0 });

        DrawRequest request = { layout, program };
        Submit(view, request);
    }

    void GraphicsContext::Quad(
        GraphicsContext::LayoutStream& input, const cVector3f& pos, const cVector2f& size, const cVector2f& uv0, const cVector2f& uv1) {
        BX_ASSERT(bgfx::getCaps(), "GraphicsContext::Init() must be called before ScreenSpaceQuad()");

        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 4, layout::PositionTexCoord0::layout());
        auto* vertex = reinterpret_cast<layout::PositionTexCoord0*>(vb.data);

        const float minx = pos.x;
        const float maxx = pos.x + size.x;
        const float miny = pos.y;
        const float maxy = pos.y + size.y;

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = pos.z;
        vertex[0].m_u = uv0.x;
        vertex[0].m_v = uv0.y;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = pos.z;
        vertex[1].m_u = uv1.x;
        vertex[1].m_v = uv0.y;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = pos.z;
        vertex[2].m_u = uv1.x;

        vertex[3].m_x = minx;
        vertex[3].m_y = maxy;
        vertex[3].m_z = pos.z;
        vertex[3].m_u = uv0.x;
        vertex[3].m_v = uv1.y;

        input.m_vertexStreams.push_back({
            .m_transient = vb,
        });
    }

    void GraphicsContext::ScreenSpaceQuad(
        GraphicsContext::LayoutStream& input, cMatrixf& proj, float textureWidth, float textureHeight, float width, float height) {
        BX_ASSERT(bgfx::getCaps(), "GraphicsContext::Init() must be called before ScreenSpaceQuad()");

        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, layout::PositionTexCoord0::layout());
        auto* vertex = reinterpret_cast<layout::PositionTexCoord0*>(vb.data);

        bx::mtxOrtho(proj.v, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

        const float minx = -width;
        const float maxx = width;
        const float miny = 0.0f;
        const float maxy = height * 2.0f;

        const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
        const float texelHalf = bgfx::RendererType::Direct3D9 == renderer ? 0.5f : 0.0f;

        const float texelHalfW = texelHalf / textureWidth;
        const float texelHalfH = texelHalf / textureHeight;
        const float minu = -1.0f + texelHalfW;
        const float maxu = 1.0f + texelHalfH;

        const float zz = 0.0f;

        float minv = texelHalfH;
        float maxv = 2.0f + texelHalfH;

        if (bgfx::getCaps()->originBottomLeft) {
            const float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        input.m_vertexStreams.push_back({
            .m_transient = vb,
        });
    }

} // namespace hpl
