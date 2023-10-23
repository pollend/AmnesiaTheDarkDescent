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

#ifndef HPL_DUMMY_RENDERABLE_H
#define HPL_DUMMY_RENDERABLE_H

#include "graphics/DrawPacket.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Renderable.h"

namespace hpl {

	//------------------------------------------

	class cCamera;
	class cFrustum;
	class iTexture;
	class cResources;

	//------------------------------------------

	class cDummyRenderable : public iRenderable
	{
	public:
		cDummyRenderable(tString asName);
		virtual ~cDummyRenderable();

		//////////////////////////////
		//Properties

		//////////////////////////////
		//iEntity implementation
		virtual tString GetEntityType() override{ return "cDummy";}

		///////////////////////////////
		//Renderable implementation:
		virtual cMaterial *GetMaterial() override { return NULL;}
		virtual iVertexBuffer* GetVertexBuffer() override{ return NULL;}

        virtual DrawPacket ResolveDrawPacket(const ForgeRenderer::Frame& frame,std::span<eVertexBufferElement> elements) override {
            DrawPacket packet;
            packet.m_type = DrawPacket::Unknown;
            return packet;
        }

		virtual eRenderableType GetRenderType() override{ return eRenderableType_Dummy;}

		virtual int GetMatrixUpdateCount() override{ return GetTransformUpdateCount();}
		virtual cMatrixf* GetModelMatrix(cFrustum* apFrustum) override;

	private:
		cMatrixf m_mtxModelOutput;
	};

};
#endif // HPL_DUMMY_RENDERABLE_H
