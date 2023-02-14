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

#include "graphics/RenderFunctions.h"

#include "graphics/RenderList.h"
#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/RenderTarget.h"
#include "graphics/VertexBuffer.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"
#include "graphics/Graphics.h"
#include <bx/debug.h>

#include "system/LowLevelSystem.h"

#include "math/Math.h"
#include "math/Frustum.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	//-----------------------------------------------------------------------

	void iRenderFunctions::SetupRenderFunctions(iLowLevelGraphics *apLowLevelGraphics)
	{
		mpLowLevelGraphics = apLowLevelGraphics;

		mvScreenSize = mpLowLevelGraphics->GetScreenSizeInt();
		mvScreenSizeFloat = mpLowLevelGraphics->GetScreenSizeFloat();
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::InitAndResetRenderFunctions(	cFrustum *apFrustum, const RenderViewport& apRenderTarget, bool abLog,
														bool abUseGlobalScissorRect,
														const cVector2l& avGlobalScissorRectPos, const cVector2l& avGlobalScissorRectSize)
	{
		mpCurrentFrustum = apFrustum;
		m_currentRenderTarget = apRenderTarget;
		mbLog = abLog;

		////////////////////////////////
		//Get size of render target
		auto& renderTarget = m_currentRenderTarget.GetRenderTarget();
		cVector2l vFrameBufferSize = renderTarget && renderTarget->IsValid() ?  renderTarget->GetImage()->GetSize() : mvScreenSize;
		mvRenderTargetSize.x = m_currentRenderTarget.GetSize().x < 0 ? vFrameBufferSize.x : m_currentRenderTarget.GetSize().x;
		mvRenderTargetSize.y = m_currentRenderTarget.GetSize().y < 0 ? vFrameBufferSize.y : m_currentRenderTarget.GetSize().y;
		mvCurrentFrameBufferSize = vFrameBufferSize;
	}

}
