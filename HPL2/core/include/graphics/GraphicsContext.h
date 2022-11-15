#pragma once

#include <bgfx/bgfx.h>

class GraphicsContext final
{
public:
    GraphicsContext();

    void Reset();
    bgfx::ViewId StartPass();

private:
    bgfx::ViewId _current;
};
