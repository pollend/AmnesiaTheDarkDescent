#include "absl/strings/string_view.h"
#include <bgfx/bgfx.h>
#include <graphics/GraphicsContext.h>

#include <bx/debug.h>

namespace hpl
{
    struct PositionTexCoord0
    {
        float _x;
        float _y;
        float _z;
        float _u;
        float _v;

        static void init()
        {
            _layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();
        }

        static bgfx::VertexLayout _layout;
    };
    bgfx::VertexLayout PositionTexCoord0::_layout;

    GraphicsContext::GraphicsContext()
        : _current(0)
    {
    }


    uint16_t GraphicsContext::ScreenWidth() const
    {
        return 0;
    }
    uint16_t GraphicsContext::ScreenHeight() const
    {
        return 0;
    }

    void GraphicsContext::UpdateScreenSize(uint16_t width, uint16_t height) 
    {
    }


    void GraphicsContext::Init()
    {

	    // bgfx::Init init;
		// init.type     = args.m_type;
		// init.vendorId = args.m_pciId;
		// init.platformData.nwh  = entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
		// init.platformData.ndt  = entry::getNativeDisplayHandle();
		// init.resolution.width  = m_width;
		// init.resolution.height = m_height;
		// init.resolution.reset  = m_reset;
		// bgfx::init(init);

        _caps = bgfx::getCaps();
        PositionTexCoord0::init();
    }

    void GraphicsContext::Reset()
    {
        _current = 0;
    }

    bgfx::ViewId GraphicsContext::StartPass(absl::string_view name)
    {
        bgfx::ViewId view = _current++;
        bgfx::setViewName(view, name.data());
        return view;
    }

    bool GraphicsContext::isOriginBottomLeft() const { 
        BX_ASSERT(_caps, "GraphicsContext::Init() must be called before isOriginBottomLeft()");
        return _caps->originBottomLeft; 
    }
        

    void GraphicsContext::ScreenSpaceQuad(float textureWidth, float textureHeight, float width, float height)
    {
        BX_ASSERT(_caps, "GraphicsContext::Init() must be called before ScreenSpaceQuad()");

        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, PositionTexCoord0::_layout);
        PositionTexCoord0* vertex = (PositionTexCoord0*)vb.data;

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

        if (_caps->originBottomLeft)
        {
            const float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0]._x = minx;
        vertex[0]._y = miny;
        vertex[0]._z = zz;
        vertex[0]._u = minu;
        vertex[0]._v = minv;

        vertex[1]._x = maxx;
        vertex[1]._y = miny;
        vertex[1]._z = zz;
        vertex[1]._u = maxu;
        vertex[1]._v = minv;

        vertex[2]._x = maxx;
        vertex[2]._y = maxy;
        vertex[2]._z = zz;
        vertex[2]._u = maxu;
        vertex[2]._v = maxv;

		bgfx::setVertexBuffer(0, &vb);
    }

} // namespace hpl