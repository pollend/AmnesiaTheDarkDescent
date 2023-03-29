#pragma once

#include "bgfx/bgfx.h"

namespace hpl::layout {

    struct PositionColor {
        float m_pos[3];
        float m_color[4];

        static bgfx::VertexLayout& layout() {
            static bgfx::VertexLayout layout = ([]() {
                bgfx::VertexLayout inst;
                inst.begin()
                    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
                    .end();
                return inst;
            })();
            return layout;
        }
    };

    struct PositionTexCoord0 {
        float m_x;
        float m_y;
        float m_z;

        float m_u;
        float m_v;

        static bgfx::VertexLayout& layout() {
            static bgfx::VertexLayout layout = ([]() {
                bgfx::VertexLayout inst;
                inst.begin()
                    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                    .end();
                return inst;
            })();
            return layout;
        }
    };

} // namespace hpl::layout