#include "bx/math.h"
#include "engine/Event.h"
#include "engine/Interface.h"
#include "graphics/Enum.h"
#include "graphics/ShaderUtil.h"
#include "math/Math.h"
#include "math/MathTypes.h"
#include "scene/Camera.h"
#include <absl/container/inlined_vector.h>
#include <absl/strings/string_view.h>
#include <algorithm>
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <cstddef>
#include <cstdint>
#include <graphics/GraphicsContext.h>
#include <graphics/GraphicsTypes.h>

#include <bx/debug.h>
#include <variant>

namespace hpl {

    GraphicsContext::ShaderProgram::ShaderProgram() {
        m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
        m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
    }

    struct PositionColor {
        float m_pos[3];
        float m_color[4];

        static void Init() {
            m_layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
                .end();
        }
        static bgfx::VertexLayout m_layout;
    };
    bgfx::VertexLayout PositionColor::m_layout;
    
    struct PositionTexCoord0 {
        float m_x;
        float m_y;
        float m_z;

        float m_u;
        float m_v;

        static void init() {
            m_layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();
        }

        static bgfx::VertexLayout m_layout;
    };
    bgfx::VertexLayout PositionTexCoord0::m_layout;

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

        if (bgfx::isValid(m_copyProgram)) {
            bgfx::destroy(m_copyProgram);
        }
        if (bgfx::isValid(m_s_diffuseMap)) {
            bgfx::destroy(m_s_diffuseMap);
        }
        if (bgfx::isValid(m_u_normalMtx)) {
            bgfx::destroy(m_u_normalMtx);
        }
        if (bgfx::isValid(m_colorProgram)) {
            bgfx::destroy(m_colorProgram);
        }
        if (bgfx::isValid(m_uvProgram)) {
            bgfx::destroy(m_uvProgram);
        }
    }

    uint16_t GraphicsContext::ScreenWidth() const {
        return 0;
    }
    uint16_t GraphicsContext::ScreenHeight() const {
        return 0;
    }

    void GraphicsContext::UpdateScreenSize(uint16_t width, uint16_t height) {
    }

    void GraphicsContext::Init() {
        PositionTexCoord0::init();
        PositionColor::Init();
        m_copyProgram = hpl::loadProgram("vs_post_effect", "fs_post_effect_copy");
        m_colorProgram = hpl::loadProgram("vs_color", "fs_color");
        m_uvProgram = hpl::loadProgram("vs_basic_uv", "fs_basic_uv");
        m_meshColorProgram = hpl::loadProgram("vs_color", "fs_color_1");
        m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
        m_u_normalMtx = bgfx::createUniform("u_normalMtx", bgfx::UniformType::Mat4);
        m_u_color = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
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
        bgfx::allocTransientVertexBuffer(&vb, 4, PositionTexCoord0::m_layout);
        PositionTexCoord0* vertex = (PositionTexCoord0*)vb.data;

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
        bgfx::allocTransientVertexBuffer(&vb, 3, PositionTexCoord0::m_layout);
        PositionTexCoord0* vertex = (PositionTexCoord0*)vb.data;

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

    ImmediateDrawBatch::ImmediateDrawBatch(GraphicsContext& context, RenderTarget& target, const cMatrixf& view, const cMatrixf& projection):
        m_context(context),
        m_target(target),
        m_view(view),
        m_projection(projection) {
    }

    void ImmediateDrawBatch::DebugDrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, const cColor& color, const DebugDrawOptions& options) {
        DebugDrawLine(cVector3f(avMax.x, avMax.y, avMax.z), cVector3f(avMin.x, avMax.y, avMax.z), color, options);

        DebugDrawLine(cVector3f(avMax.x, avMax.y, avMax.z), cVector3f(avMax.x, avMin.y, avMax.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMax.y, avMax.z), cVector3f(avMin.x, avMin.y, avMax.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMin.y, avMax.z), cVector3f(avMax.x, avMin.y, avMax.z), color, options);

        // Neg Z Quad
        DebugDrawLine(cVector3f(avMax.x, avMax.y, avMin.z), cVector3f(avMin.x, avMax.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMax.x, avMax.y, avMin.z), cVector3f(avMax.x, avMin.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMax.y, avMin.z), cVector3f(avMin.x, avMin.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMin.y, avMin.z), cVector3f(avMax.x, avMin.y, avMin.z), color, options);

        // Lines between
        DebugDrawLine(cVector3f(avMax.x, avMax.y, avMax.z), cVector3f(avMax.x, avMax.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMax.y, avMax.z), cVector3f(avMin.x, avMax.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMin.x, avMin.y, avMax.z), cVector3f(avMin.x, avMin.y, avMin.z), color, options);

        DebugDrawLine(cVector3f(avMax.x, avMin.y, avMax.z), cVector3f(avMax.x, avMin.y, avMin.z), color, options);
    }

    void ImmediateDrawBatch::DebugDrawLine(const cVector3f& start, const cVector3f& end, const cColor& color, const DebugDrawOptions& options) {
        cVector3f transformStart =  cMath::MatrixMul(options.m_transform, start);
        cVector3f transformEnd =  cMath::MatrixMul(options.m_transform, end);
        m_lineSegments.push_back(LineSegmentRequest {
            .m_depthTest = options.m_depthTest,
            .m_start = {transformStart.x, transformStart.y, transformStart.z},
            .m_end = {transformEnd.x, transformEnd.y, transformEnd.z},
            .m_color = {color.r, color.g, color.b, color.a},
        });
    }
    void ImmediateDrawBatch::DebugDrawMesh(const GraphicsContext::LayoutStream& layout, const cColor& color, const DebugDrawOptions& options) {
        m_debugMeshes.push_back({
            .m_layout = layout,
            .m_depthTest = options.m_depthTest,
            .m_transform = options.m_transform,
            .m_color = {color.r, color.g, color.b, color.a},
        });
    }

    void ImmediateDrawBatch::DrawQuad(const cVector3f& v1, const cVector3f& v2, const cVector3f& v3, const cVector2f& uv0,const cVector2f& uv1, hpl::Image* image , const cColor& aTint, const DebugDrawOptions& options) {
        DrawQuad(
            Eigen::Vector3f(v1.x, v1.y, v1.z), 
            Eigen::Vector3f(v2.x, v2.y, v2.z), 
            Eigen::Vector3f(v3.x, v3.y, v3.z), 
            Eigen::Vector2f(uv0.x,uv0.y), 
            Eigen::Vector2f(uv1.x,uv1.y), 
            image, {aTint.r, aTint.g, aTint.b, aTint.a}, options);
    }


    void ImmediateDrawBatch::DrawQuad(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const Eigen::Vector3f& v3, const Eigen::Vector2f& uv0, const Eigen::Vector2f& uv1, hpl::Image* image, const Eigen::Vector4f& aTint, const DebugDrawOptions& options) {
        m_uvQuads.push_back(UVQuadRequest {
            .m_depthTest = options.m_depthTest,
            .m_v1 = {v1.x(), v1.y(), v1.z()},
            .m_v2 = {v2.x(), v2.y(), v2.z()},
            .m_v3 = {v3.x(), v3.y(), v3.z()},
            .m_uv0 = {uv0.x(), uv0.y()},
            .m_uv1 = {uv1.x(), uv1.y()},
            .m_uvImage = image,
            .m_color = {aTint.x(), aTint.y(), aTint.z(), aTint.w()},
        });
    }
    void ImmediateDrawBatch::DrawQuad(const cVector3f& v1, const cVector3f& v2, const cVector3f& v3, const cColor& aColor, const DebugDrawOptions& options) {
        m_colorQuads.push_back(ColorQuadRequest {
            .m_depthTest = options.m_depthTest,
            .m_v1 = {v1.x, v1.y, v1.z},
            .m_v2 = {v2.x, v2.y, v2.z},
            .m_v3 = {v3.x, v3.y, v3.z},
            .m_color = {aColor.r, aColor.g, aColor.b, aColor.a},
        });
    }

    void ImmediateDrawBatch::DrawQuad(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const Eigen::Vector3f& v3, const Eigen::Vector4f& color, const DebugDrawOptions& options) {
        m_colorQuads.push_back(ColorQuadRequest {
            .m_depthTest = options.m_depthTest,
            .m_v1 = {v1.x(), v1.y(), v1.z()},
            .m_v2 = {v2.x(), v2.y(), v2.z()},
            .m_v3 = {v3.x(), v3.y(), v3.z()},
            .m_color = {color.x(), color.y(), color.z(), color.w()},
        });
    }

    void ImmediateDrawBatch::DrawBillboard(const cVector3f& pos, const cVector2f& size, const cVector2f& uv0, const cVector2f& uv1, hpl::Image* image, const cColor& aTint, const DebugDrawOptions& options){
        DrawBillboard(
            Eigen::Vector3f(pos.x, pos.y, pos.z), 
            Eigen::Vector2f(size.x, size.y), 
            Eigen::Vector2f(uv0.x, uv0.y), 
            Eigen::Vector2f(uv1.x, uv1.y),
            image,
            Eigen::Vector4f(aTint.r, aTint.g, aTint.b, aTint.a), options);
    }

    // scale based on distance from camera
    float ImmediateDrawBatch::BillboardScale(cCamera* apCamera, const Eigen::Vector3f& pos) {
        const auto avViewSpacePosition = cMath::MatrixMul(apCamera->GetViewMatrix(), cVector3f(pos.x(), pos.y(), pos.z()));
        switch(apCamera->GetProjectionType())
		{
		case eProjectionType_Orthographic:
			return apCamera->GetOrthoViewSize().x * 0.25f;
		case eProjectionType_Perspective:
			return cMath::Abs(avViewSpacePosition.z);
		default:
			break;
		}
        BX_ASSERT(false, "invalid projection type");
        return 0.0f;
		
    } 



    void ImmediateDrawBatch::DrawBillboard(const Eigen::Vector3f& pos, const Eigen::Vector2f& size, const Eigen::Vector2f& uv0, const Eigen::Vector2f& uv1, hpl::Image* image, const Eigen::Vector4f& aTint, const DebugDrawOptions& options) {
        Eigen::Matrix4f rotation = Eigen::Matrix4f::Identity();
        rotation.block<3,3>(0,0) = Eigen::Matrix3f({
            {m_view.m[0][0], m_view.m[0][1], m_view.m[0][2]},
            {m_view.m[1][0], m_view.m[1][1], m_view.m[1][2]},
            {m_view.m[2][0], m_view.m[2][1], m_view.m[2][2]}
        });

        Eigen::Matrix4f billboard = Eigen::Matrix4f::Identity();
        billboard.block<3,1>(0,3) = pos;
        Eigen::Matrix4f transform = billboard * rotation;

        Eigen::Vector2f halfSize = Eigen::Vector2f(size.x()/2.0f, size.y()/2.0f);
        Eigen::Vector4f v1 = (transform * Eigen::Vector4f(halfSize.x(), halfSize.y(), 0, 1));
        Eigen::Vector4f v2 = (transform * Eigen::Vector4f(-halfSize.x(), halfSize.y(), 0, 1));
        Eigen::Vector4f v3 = (transform * Eigen::Vector4f(halfSize.x(), -halfSize.y(), 0, 1));
        DrawQuad(v1.head<3>(), v2.head<3>(), v3.head<3>(), uv0, uv1, image, aTint, options);


        // DrawQuad(Eigen::Vector3f(pos.x(), pos.y(), pos.z()), Eigen::Vector3f(pos.x() + 10, pos.y(), pos.z()), Eigen::Vector3f(pos.x(), pos.y() + 10, pos.z()), uv0, uv1, image, aTint, options);
    }

    void ImmediateDrawBatch::DebugDrawSphere(const cVector3f& pos, float radius, const cColor& color, const DebugDrawOptions& options) {
        
		constexpr int alSegments = 32;
		constexpr float afAngleStep = k2Pif /static_cast<float>(alSegments);
        //X Circle:
        for(float a=0; a< k2Pif; a+= afAngleStep)
        {
            DebugDrawLine(cVector3f(pos.x, pos.y + sin(a)*radius,
                pos.z + cos(a)*radius), cVector3f(pos.x, pos.y + sin(a+afAngleStep)*radius,
                pos.z + cos(a+afAngleStep)*radius), color, options);
        }

        //Y Circle:
        for(float a=0; a< k2Pif; a+= afAngleStep)
        {
            DebugDrawLine(
                cVector3f(pos.x + cos(a)*radius, pos.y, pos.z + sin(a)*radius), 
                cVector3f(pos.x + cos(a+afAngleStep)*radius, pos.y,pos.z+ sin(a+afAngleStep)*radius), color, options);
        }

        //Z Circle:
        for(float a=0; a< k2Pif; a+= afAngleStep)
        {
            DebugDrawLine(
                cVector3f(pos.x + cos(a)*radius, pos.y + sin(a)*radius, pos.z), 
                cVector3f(pos.x + cos(a+afAngleStep)*radius, pos.y + sin(a+afAngleStep)*radius, pos.z), color, options);
        }
    }

    void ImmediateDrawBatch::flush() {
        if(m_lineSegments.empty() && m_colorQuads.empty() && m_uvQuads.empty()) {
            return;
        }

        cVector2l imageSize = m_target.GetImage()->GetImageSize();
        GraphicsContext::ViewConfiguration viewConfiguration {m_target};
        viewConfiguration.m_viewRect = { 0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y) };
        viewConfiguration.m_projection = m_projection;
        viewConfiguration.m_view = m_view;
        const auto view = m_context.StartPass("Immediate - Lines", viewConfiguration);
        

        if(!m_lineSegments.empty())
        {
            std::sort(m_lineSegments.begin(), m_lineSegments.end(), [](const LineSegmentRequest& a, const LineSegmentRequest& b) {
                return a.m_depthTest < b.m_depthTest;
            });

            const size_t numVertices = m_lineSegments.size() * 2;

            bgfx::TransientVertexBuffer vb;
            bgfx::TransientIndexBuffer ib;
            bgfx::allocTransientVertexBuffer(&vb, numVertices, PositionColor::m_layout);
            bgfx::allocTransientIndexBuffer(&ib, numVertices);
            
            auto it = m_lineSegments.begin();
            auto lastIt = m_lineSegments.begin();
            size_t vertexBufferOffset = 0;
            size_t indexBufferOffset = 0;
            while(it != m_lineSegments.end()) {
                
                size_t vertexBufferIndex = 0;
                size_t indexBufferIndex = 0;
                do
                {
                    {
                        reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                        auto& posColor = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)];
                        posColor.m_pos[0] = it->m_start.x();
                        posColor.m_pos[1] = it->m_start.y();
                        posColor.m_pos[2] = it->m_start.z();

                        posColor.m_color[0] = it->m_color.x();
                        posColor.m_color[1] = it->m_color.y();
                        posColor.m_color[2] = it->m_color.z();
                        posColor.m_color[3] = it->m_color.w();
                    }

                    {
                        reinterpret_cast<uint16_t*>(ib.data)[indexBufferOffset + (indexBufferIndex++)] = vertexBufferIndex;
                        auto& posColor = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset + (vertexBufferIndex++)];
                        posColor.m_pos[0] = it->m_end.x();
                        posColor.m_pos[1] = it->m_end.y();
                        posColor.m_pos[2] = it->m_end.z();

                        posColor.m_color[0] = it->m_color.x();
                        posColor.m_color[1] = it->m_color.y();
                        posColor.m_color[2] = it->m_color.z();
                        posColor.m_color[3] = it->m_color.w();
                    }

                    lastIt = it;
                    it++;
                } while(it != m_lineSegments.end() && 
                    it->m_depthTest == lastIt->m_depthTest);
                
                GraphicsContext::LayoutStream layout;
                layout.m_vertexStreams.push_back({
                    .m_transient = vb,
                    .m_startVertex = static_cast<uint32_t>(vertexBufferOffset),
                    .m_numVertices = static_cast<uint32_t>(vertexBufferIndex)
                });
                layout.m_indexStream = {
                    .m_transient = ib,
                    .m_startIndex = static_cast<uint32_t>(indexBufferOffset),
                    .m_numIndices = static_cast<uint32_t>(indexBufferIndex)
                };
                indexBufferOffset += indexBufferIndex;
                vertexBufferOffset += vertexBufferIndex;

                layout.m_drawType = eVertexBufferDrawType_Line;
                
                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_handle = m_context.m_colorProgram;
                shaderProgram.m_configuration.m_depthTest = lastIt->m_depthTest;
                shaderProgram.m_configuration.m_rgbBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_configuration.m_alphaBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_configuration.m_write = Write::RGB;
                // shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;

                GraphicsContext::DrawRequest request = {
                    layout,
                    shaderProgram,
                };
                m_context.Submit(view, request);   
            }
        }
        if(m_debugMeshes.size() > 0) {
            for(auto& mesh: m_debugMeshes) {
               
                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_handle = m_context.m_meshColorProgram;
                shaderProgram.m_configuration.m_depthTest = mesh.m_depthTest;
                shaderProgram.m_configuration.m_rgbBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_configuration.m_alphaBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                struct {
                    float m_r;
                    float m_g;
                    float m_b;
                    float m_a;
                } color = {
                    mesh.m_color.r,
                    mesh.m_color.g,
                    mesh.m_color.b,
                    mesh.m_color.a
                };
                shaderProgram.m_uniforms.push_back({m_context.m_u_color, &color});
                shaderProgram.m_configuration.m_write = Write::RGB;
                shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
                GraphicsContext::DrawRequest request = {
                    mesh.m_layout,
                    shaderProgram,
                };
                m_context.Submit(view, request);   
            }
        }

        if(!m_uvQuads.empty()) {
            size_t vertexBufferOffset = 0;
            size_t indexBufferOffset = 0;
            cVector2l imageSize = m_target.GetImage()->GetImageSize();
            GraphicsContext::ViewConfiguration viewConfiguration {m_target};
            viewConfiguration.m_viewRect = { 0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y) };
            viewConfiguration.m_projection = m_projection;
            viewConfiguration.m_view = m_view;
            const auto view = m_context.StartPass("Immediate - Quad", viewConfiguration);
            const size_t numVertices = m_colorQuads.size() * 4;

            bgfx::TransientIndexBuffer ib;
            bgfx::allocTransientIndexBuffer(&ib, 6);
		    uint16_t* indexBuffer = reinterpret_cast<uint16_t*>(ib.data);
            indexBuffer[0] = 0;
            indexBuffer[1] = 1;
            indexBuffer[2] = 2;
            indexBuffer[3] = 1;
            indexBuffer[4] = 2;
            indexBuffer[5] = 3;

            for(auto& quad: m_uvQuads) {
                bgfx::TransientVertexBuffer vb;
                bgfx::allocTransientVertexBuffer(&vb, 4, PositionTexCoord0::m_layout);
                PositionTexCoord0* vertexBuffer = reinterpret_cast<PositionTexCoord0*>(vb.data);

                Eigen::Vector3f v1v2 = quad.m_v2 - quad.m_v1;
                Eigen::Vector3f v1v3 = quad.m_v3 - quad.m_v1;
                Eigen::Vector3f v4 = quad.m_v1 + v1v2 + v1v3;
                vertexBuffer[0] = {
                    .m_x = quad.m_v1.x(),
                    .m_y = quad.m_v1.y(),
                    .m_z = quad.m_v1.z(),
                    .m_u = quad.m_uv0.x(),
                    .m_v = quad.m_uv0.y()
                };
                vertexBuffer[1] = {
                    .m_x = quad.m_v2.x(),
                    .m_y = quad.m_v2.y(),
                    .m_z = quad.m_v2.z(),
                    .m_u = quad.m_uv1.x(),
                    .m_v = quad.m_uv0.y()
                };
                vertexBuffer[2] = {
                    .m_x = quad.m_v3.x(),
                    .m_y = quad.m_v3.y(),
                    .m_z = quad.m_v3.z(),
                    .m_u = quad.m_uv0.x(),
                    .m_v = quad.m_uv1.y()
                };
                vertexBuffer[3] = {
                    .m_x = v4.x(),
                    .m_y = v4.y(),
                    .m_z = v4.z(),
                    .m_u = quad.m_uv1.x(),
                    .m_v = quad.m_uv1.y()
                };
            
                struct {
                    float m_r;
                    float m_g;
                    float m_b;
                    float m_a;
                } color = {
                    quad.m_color.r,
                    quad.m_color.g,
                    quad.m_color.b,
                    quad.m_color.a
                };

                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_handle = m_context.m_uvProgram;
                shaderProgram.m_configuration.m_depthTest = quad.m_depthTest;
                
                // shaderProgram.m_configuration.m_cull = Cull::None;
                shaderProgram.m_configuration.m_rgbBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_configuration.m_alphaBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_uniforms.push_back({m_context.m_u_color, &color});
                shaderProgram.m_textures.push_back({m_context.m_s_diffuseMap, quad.m_uvImage->GetHandle(), 0});
                shaderProgram.m_configuration.m_write = Write::RGB;
                // shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
                
                GraphicsContext::LayoutStream layout;
                layout.m_vertexStreams.push_back({
                    .m_transient = vb,
                    // .m_startVertex = static_cast<uint32_t>(0),
                    // .m_numVertices = static_cast<uint32_t>(4)
                });
                layout.m_indexStream = {
                    .m_transient = ib,
                    // .m_startIndex = static_cast<uint32_t>(0),
                    // .m_numIndices = static_cast<uint32_t>(4)
                };

                GraphicsContext::DrawRequest request = {
                    layout,
                    shaderProgram,
                };
                m_context.Submit(view, request); 
            }
        }

        if(!m_colorQuads.empty()) {
            size_t vertexBufferOffset = 0;
            size_t indexBufferOffset = 0;
            cVector2l imageSize = m_target.GetImage()->GetImageSize();
            GraphicsContext::ViewConfiguration viewConfiguration {m_target};
            viewConfiguration.m_viewRect = { 0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y) };
            viewConfiguration.m_projection = m_projection;
            viewConfiguration.m_view = m_view;
            const auto view = m_context.StartPass("Immediate - Color Quad", viewConfiguration);
            const size_t numVertices = m_colorQuads.size() * 4;

            bgfx::TransientVertexBuffer vb;
            bgfx::TransientIndexBuffer ib;
            bgfx::allocTransientVertexBuffer(&vb, numVertices, PositionColor::m_layout);
            bgfx::allocTransientIndexBuffer(&ib, 4);

            for(auto& quad: m_colorQuads) {
                Eigen::Vector3f v1v2 = quad.m_v2 - quad.m_v1;
                Eigen::Vector3f v1v3 = quad.m_v3 - quad.m_v1;
                Eigen::Vector3f v4 = quad.m_v1 + v1v2 + v1v3;
                {
                    auto& posTex = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset++];
                    posTex.m_pos[0] = quad.m_v1.x();
                    posTex.m_pos[1] = quad.m_v1.y();
                    posTex.m_pos[2] = quad.m_v1.z();

                    posTex.m_color[0] = quad.m_color.x();
                    posTex.m_color[1] = quad.m_color.y();
                    posTex.m_color[2] = quad.m_color.z();
                    posTex.m_color[3] = quad.m_color.w();
                }
                {
                    auto& posTex = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset++];
                    posTex.m_pos[0] = quad.m_v2.x();
                    posTex.m_pos[1] = quad.m_v2.y();
                    posTex.m_pos[2] = quad.m_v2.z();

                    posTex.m_color[0] = quad.m_color.x();
                    posTex.m_color[1] = quad.m_color.y();
                    posTex.m_color[2] = quad.m_color.z();
                    posTex.m_color[3] = quad.m_color.w();
                }
                {
                    auto& posTex = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset++];
                    posTex.m_pos[0] = quad.m_v3.x();
                    posTex.m_pos[1] = quad.m_v3.y();
                    posTex.m_pos[2] = quad.m_v3.z();

                    posTex.m_color[0] = quad.m_color.x();
                    posTex.m_color[1] = quad.m_color.y();
                    posTex.m_color[2] = quad.m_color.z();
                    posTex.m_color[3] = quad.m_color.w();
                }
                {
                    auto& posTex = reinterpret_cast<PositionColor*>(vb.data)[vertexBufferOffset++];
                    posTex.m_pos[0] = v4.x();
                    posTex.m_pos[1] = v4.y();
                    posTex.m_pos[2] = v4.z();

                    posTex.m_color[0] = quad.m_color.x();
                    posTex.m_color[1] = quad.m_color.y();
                    posTex.m_color[2] = quad.m_color.z();
                    posTex.m_color[3] = quad.m_color.w();
                }

                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_handle = m_context.m_colorProgram;
                shaderProgram.m_configuration.m_depthTest = quad.m_depthTest;
                shaderProgram.m_configuration.m_rgbBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                shaderProgram.m_configuration.m_alphaBlendFunc = 
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                // shaderProgram.m_uniforms.push_back({m_context.m_u_color, &color});

                GraphicsContext::LayoutStream layout;
                layout.m_vertexStreams.push_back({
                    .m_transient = vb,
                    .m_startVertex = static_cast<uint32_t>(vertexBufferOffset),
                    .m_numVertices = static_cast<uint32_t>(4)
                });

                GraphicsContext::DrawRequest request = {
                    layout,
                    shaderProgram,
                };
                m_context.Submit(view, request); 
            }
        }

    }

} // namespace hpl
