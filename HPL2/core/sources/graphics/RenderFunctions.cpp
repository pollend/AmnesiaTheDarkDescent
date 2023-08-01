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

#include "graphics/FrameBuffer.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/RenderList.h"
#include "graphics/RenderTarget.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"
#include "system/LowLevelSystem.h"

#include "math/Frustum.h"
#include "math/Math.h"

namespace hpl {

    void iRenderFunctions::SetupRenderFunctions(iLowLevelGraphics* apLowLevelGraphics) {
        mpLowLevelGraphics = apLowLevelGraphics;
    }

    //-----------------------------------------------------------------------

    void iRenderFunctions::InitAndResetRenderFunctions(
        cFrustum* apFrustum,
        bool abLog,
        bool abUseGlobalScissorRect,
        const cVector2l& avGlobalScissorRectPos,
        const cVector2l& avGlobalScissorRectSize) {
        mpCurrentFrustum = apFrustum;
    }

} // namespace hpl
