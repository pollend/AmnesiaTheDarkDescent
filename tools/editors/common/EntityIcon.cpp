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

#include "EntityIcon.h"
#include "EntityWrapper.h"
#include "EditorWorld.h"
#include "EditorHelper.h"
#include "EditorWindowViewport.h"

#include "graphics/ImmediateDrawBatch.h"

cEntityIcon::cEntityIcon(iEntityWrapper* apParent, const tString& asIconGfxName)
{
	mbVisible = true;

	mpParent = apParent;
	for(auto& img: mvIconGfx) {
		img = nullptr;
	}

	cTextureManager* pTexMgr = apParent->GetEditorWorld()->GetEditor()->GetEngine()->GetResources()->GetTextureManager();
	mvIconGfx[0] = pTexMgr->Create2DImage("Billboard" + asIconGfxName + ".tga", true);
	mvIconGfx[1] = pTexMgr->Create2DImage("Billboard" + asIconGfxName + "Selected.tga", true);
}

//------------------------------------------------------------------

cEntityIcon::~cEntityIcon()
{
	cTextureManager* pTexMgr = mpParent->GetEditorWorld()->GetEditor()->GetEngine()->GetResources()->GetTextureManager();

	for(auto& img: mvIconGfx) {
		if(img) {
			pTexMgr->Destroy(img);
		}
	}
}

//------------------------------------------------------------------

bool cEntityIcon::Check2DBoxIntersect(cEditorWindowViewport* apViewport, const cRect2l& aBox)
{
	cRect2l rect;
	cFrustum* pFrustum = apViewport->GetCamera()->GetFrustum();
	cBoundingVolume* pBV = GetPickBV(apViewport, cVector3f(0.06f));

	if(pFrustum->CollideBoundingVolume(pBV)== eCollision_Outside)
		return false;

	cMath::GetClipRectFromBV(rect,*pBV,pFrustum,apViewport->GetGuiViewportSizeInt(), pFrustum->GetFOV()*0.5f);

	return cMath::CheckRectIntersection(aBox, rect);
}

bool cEntityIcon::CheckRayIntersect(cEditorWindowViewport* apViewport, cVector3f* apIntersection, tVector3fVec* apTriangle, float *apT)
{
	cBoundingVolume* pBV = GetPickBV(apViewport, 0.06f);
	return cMath::CheckBVIntersection(*apViewport->GetRayBV(), *pBV) &&
		cMath::CheckAABBLineIntersection(pBV->GetMin(), pBV->GetMax(), apViewport->GetUnprojectedStart(),
											apViewport->GetUnprojectedEnd(), apIntersection, apT);
}

//------------------------------------------------------------------

void cEntityIcon::DrawIcon(cEditorWindowViewport* apViewport,
						   ImmediateDrawBatch* apFunctions,
						   iEditorEditMode* apEditMode,
						   bool abIsSelected,
						   const cVector3f& avPos,
						   bool abIsActive,
						   const cColor& aDisabledCol)
{
	if(mbVisible==false)
		return;

	if(mvIconGfx[abIsSelected])
	{
		cColor bbColor = abIsSelected ? cColor(1,1) : (abIsActive ? cColor(0.5f,1) : aDisabledCol);

		float scale = ImmediateDrawBatch::BillboardScale(apViewport->GetCamera(), Eigen::Vector3f(avPos.x, avPos.y, avPos.z));
		apFunctions->DrawBillboard(cMath::ToForgeVec3(avPos),
			                 cMath::ToForgeVec2(cVector2f(0.1f,0.1f) * scale),
			                 cMath::ToForgeVec2(cVector2f(1.f, 0.f)), cMath::ToForgeVec2(cVector2f(0.f, 1.f)),
			                 mvIconGfx[abIsSelected]->GetTexture(), cMath::ToForgeVec4(bbColor));
	}
}

//-------------------------------------------------------------------

cRect2l cEntityIcon::GetIconClipRectangle(cEditorWindowViewport* apViewport, iEntityWrapper* apEntity)
{
	cVector3f vUpperLeftCorner =  apEntity->GetViewSpacePosition(apViewport) - cVector3f(0.1f,0.1f,0);

	return cRect2l((int)vUpperLeftCorner.x, (int)vUpperLeftCorner.y, 1, 1);
}

//-------------------------------------------------------------------

cBoundingVolume* cEntityIcon::GetPickBV(cEditorWindowViewport* apViewport, const cVector3f& avSize)
{
	mIconBV.SetPosition(mpParent->GetPosition());

	if(apViewport==NULL)
	{
		mIconBV.SetSize(0.06f);
	}
	else
	{
		cVector3f vPosition, vSize;
		cEditorHelper::GetViewSpacePosAndSize(apViewport->GetCamera(), mpParent->GetPosition(),avSize, vPosition, vSize);

		mIconBV.SetSize(vSize);
	}

	return &mIconBV;
}

