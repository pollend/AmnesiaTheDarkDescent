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

#ifndef HPL_RENDERER_WIRE_FRAME_H
#define HPL_RENDERER_WIRE_FRAME_H

#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "graphics/Renderer.h"

namespace hpl {

    //---------------------------------------------

	class iFrameBuffer;
	class iDepthStencilBuffer;
	class iTexture;
	class iLight;

	//---------------------------------------------

	class cRendererWireFrame : public  iRenderer
	{
	public:
		cRendererWireFrame(cGraphics *apGraphics,cResources* apResources);
		~cRendererWireFrame();

		bool LoadData() override;
		void DestroyData() override;

		virtual Texture* GetOutputImage(cViewport& viewport) override {
			return nullptr;
		}
	private:

		virtual void Draw(const ForgeRenderer::Frame& context, cViewport& viewport, float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings,
		bool abSendFrameBufferToPostEffects) override;

        UniqueViewportData<LegacyRenderTarget> m_boundOutputBuffer;
        bgfx::ProgramHandle m_colorProgram;
        UniformWrapper<StringLiteral("u_color"),      bgfx::UniformType::Vec4> m_u_color;

		void CopyToFrameBuffer();
		void RenderObjects();


	};

	//---------------------------------------------

};
#endif // HPL_RENDERER_WIRE_FRAME_H
