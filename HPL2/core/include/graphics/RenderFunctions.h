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

#include "graphics/GraphicsTypes.h"
#include "graphics/RenderTarget.h"
#include "graphics/RenderViewport.h"
#include "math/MathTypes.h"
#include <cstdint>
#include <memory>

namespace hpl {

	//---------------------------------------------

	class iLowLevelGraphics;
	class iVertexBuffer;
	class iTexture;
	class iGpuProgram;
	class cFrustum;
	class cMaterial;
	class iMaterialType;
	class cGraphics;

	//---------------------------------------------

	class iRenderFunctions
	{
	public:
		virtual ~iRenderFunctions() { }

		/*
		 * This function must be set before the render functions can be used!
	     */
		void SetupRenderFunctions(iLowLevelGraphics *apLowLevelGraphics);


		/**
		 * This must be called every frame before any render function is called
		 */
		void InitAndResetRenderFunctions(	cFrustum *apFrustum, const RenderViewport& apRenderTarget, bool abLog,
											bool abUseGlobalScissorRect=false,
											const cVector2l& avGlobalScissorRectPos=0, const cVector2l& avGlobalScissorRectSize=0);


		const cVector2l& GetCurrentFrameBufferSize(){ return mvCurrentFrameBufferSize;}
		const cVector2l& GetRenderTargetSize(){ return mvRenderTargetSize;}

	protected:
		cGraphics *mpGraphics;
		iLowLevelGraphics *mpLowLevelGraphics;

		cFrustum *mpCurrentFrustum;
		RenderViewport m_currentRenderTarget;

		cVector2l mvRenderTargetSize;		//Use this when ever some calculations involving the size of rendertarget is involved!
		cVector2l mvCurrentFrameBufferSize;	//This shall NOT be used in calculations and is simply as a helper

		cVector2l mvScreenSize;
		cVector2f mvScreenSizeFloat;
	};

	//---------------------------------------------

};
