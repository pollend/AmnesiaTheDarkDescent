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

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"

#include "scene/Entity3D.h"
#include "graphics/Renderable.h"

namespace hpl {

	class cResources;
	class cGraphics;
	class cGui;

	class cGuiSet;

	//------------------------------------------

	class cGuiSetEntity : public iRenderable
	{
	public:
		cGuiSetEntity(const tString asName,cGuiSet *apSet);
		~cGuiSetEntity();

        void SetGuiSet(cGuiSet *apSet);
		cGuiSet* GetGuiSet(){ return mpGuiSet;}

		/////////////////////////////////
		//Entity implementation
		tString GetEntityType(){ return "GuiSetEntity";}

		/////////////////////////////////
		//Renderable implementations
		cMaterial *GetMaterial(){ return NULL;}
		iVertexBuffer* GetVertexBuffer(){return NULL;}
        virtual DrawPacket ResolveDrawPacket(const ForgeRenderer::Frame& frame,std::span<eVertexBufferElement> elements) { return DrawPacket();}

		eRenderableType GetRenderType(){ return eRenderableType_GuiSet;}
		cMatrixf* GetModelMatrix(cFrustum *apFrustum);
		int GetMatrixUpdateCount();

	private:
		cGuiSet *mpGuiSet;

		cMatrixf m_mtxTemp;
	};

};
