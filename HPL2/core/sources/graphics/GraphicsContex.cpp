#include <graphics/GraphicsContext.h>

namespace hpl {

GraphicsContext::GraphicsContext()
    : _current(0)
{
}

void GraphicsContext::Reset()
{
    _current = 0;
}

bgfx::ViewId GraphicsContext::StartPass()
{
    return ++_current;
}

}