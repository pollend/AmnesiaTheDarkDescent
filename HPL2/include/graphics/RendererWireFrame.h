/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "graphics/Color.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "graphics/Renderer.h"
#include <graphics/RenderList.h>
#include <memory>

namespace hpl {

	class iFrameBuffer;
	class iDepthStencilBuffer;
	class iTexture;
	class iLight;

	class cRendererWireFrame : public  iRenderer
	{
	public:
        static constexpr uint32_t NumberOfPerFrameUniforms = 64;
        static constexpr uint32_t MaxObjectUniforms = 4096;

        struct PerFrameUniform {
            mat4 m_viewProject;
        };
        struct ObjectUniform {
            mat4 m_modelMat;
        };

		cRendererWireFrame(
		    cGraphics *apGraphics,
		    cResources* apResources,
		    std::shared_ptr<DebugDraw> debug);
		~cRendererWireFrame();

        struct ViewportData {
        public:
            ViewportData() = default;
            ViewportData(const ViewportData&) = delete;
            ViewportData(ViewportData&& buffer)
                : m_size(buffer.m_size)
                , m_outputBuffer(std::move(buffer.m_outputBuffer))
                , m_clearColor(buffer.m_clearColor) {
            }

            ViewportData& operator=(const ViewportData&) = delete;

            void operator=(ViewportData&& buffer) {
                m_size = buffer.m_size;
                m_outputBuffer = std::move(buffer.m_outputBuffer);
                m_clearColor = buffer.m_clearColor;
            }

            cColor m_clearColor = cColor(0, 0);
            cVector2l m_size = cVector2l(0, 0);
            std::array<SharedRenderTarget, ForgeRenderer::SwapChainLength> m_outputBuffer;
        };
		virtual SharedRenderTarget GetOutputImage(uint32_t frameIndex, cViewport& viewport) override;

	private:
        uint32_t prepareObjectData(
            const ForgeRenderer::Frame& frame,
            iRenderable* apObject
        );

        virtual void Draw(
            Cmd* cmd,
            const ForgeRenderer::Frame& context,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings,
        RenderTargetScopedBarrier& output) override;

        std::array<SharedBuffer, NumberOfPerFrameUniforms> m_frameBufferUniform;
        uint32_t m_perFrameIndex = 0;
        uint32_t m_activeFrame = 0;
        uint32_t m_objectIndex = 0;

        SharedDescriptorSet m_constDescriptorSet;
        SharedDescriptorSet m_frameDescriptorSet;
        UniqueViewportData<ViewportData> m_boundViewportData;
        SharedRootSignature m_rootSignature;
        SharedPipeline m_pipeline;
        SharedShader m_shader;
        cRenderList m_rendererList;
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_objectUniformBuffer;
        folly::F14ValueMap<iRenderable*, uint32_t> m_objectDescriptorLookup;
        std::shared_ptr<DebugDraw> m_debug;
	};

};
