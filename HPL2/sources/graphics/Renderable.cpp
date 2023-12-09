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

#include "graphics/Renderable.h"

#include "math/Math.h"
#include "math/Frustum.h"
#include "math/cFrustum.h"
#include "system/LowLevelSystem.h"

namespace hpl {

    cRect2l iRenderable::GetClipRectFromObject(iRenderable& object,float afPaddingPercent, cFrustum* apFrustum, const cVector2l& avScreenSize) {
        cBoundingVolume* pBV = object.GetBoundingVolume();

        const float halfFovTan = tan(apFrustum->GetFOV() * 0.5f);
        cRect2l clipRect;
        cMath::GetClipRectFromBV(clipRect, *pBV, apFrustum, avScreenSize, halfFovTan);

        // Add 20% padding on clip rect
        int lXInc = (int)((float)clipRect.w * halfFovTan);
        int lYInc = (int)((float)clipRect.h * halfFovTan);

        clipRect.x = cMath::Max(clipRect.x - lXInc, 0);
        clipRect.y = cMath::Max(clipRect.y - lYInc, 0);
        clipRect.w = cMath::Min(clipRect.w + lXInc * 2, avScreenSize.x - clipRect.x);
        clipRect.h = cMath::Min(clipRect.h + lYInc * 2, avScreenSize.y - clipRect.y);

        return clipRect;
    }

    bool iRenderable::IsObjectIsVisible(iRenderable& object, tRenderableFlag neededFlags, std::span<cPlanef> clipPlanes) {
        if (!object.IsVisible())
        {
            return false;
        }

        if ((object.GetRenderFlags() & neededFlags) != neededFlags)
        {
            return false;
        }

        for (auto& plane : clipPlanes)
        {
            cBoundingVolume* pBV = object.GetBoundingVolume();
            if (cMath::CheckPlaneBVCollision(plane, *pBV) == eCollision_Outside)
            {
                return false;
            }
        }
        return true;
    }

	iRenderable::iRenderable(const tString &asName) : iEntity3D(asName)
	{

		mlLastMatrixCount = -1;

		mbStatic = false;

		mlRenderFlags = eRenderableFlag_VisibleInReflection | eRenderableFlag_VisibleInNonReflection;

		mfIlluminationAmount = 1.0f;
		mfCoverageAmount = 1.0f;

		mlRenderFrameCount = -1;

		mlCalcScaleMatrixCount = -1;
		mvCalcScale = cVector3f(1,1,1);

		mbForceShadow = false;

		mbIsOneSided = false;
		mvOneSidedNormal =0;

		mpModelMatrix = NULL;

		mfViewSpaceZ = 0;

		mbIsVisible = true;

		mlLargePlaneSurfacePlacement = 0;

		mpRenderCallback = NULL;

		mpRenderContainerNode = NULL;

		mpRenderableUserData = NULL;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iRenderable::SetRenderFlagBit(tRenderableFlag alFlagBit, bool abSet)
	{
		if(abSet)	mlRenderFlags |= alFlagBit;
		else		mlRenderFlags &= (~alFlagBit);

		if(mpRenderCallback) mpRenderCallback->OnRenderFlagsChange(this);
	}

	//-----------------------------------------------------------------------

	void iRenderable::SetVisible(bool abVisible)
	{
		mbIsVisible = abVisible;

		OnChangeVisible();

		if(mpRenderCallback) mpRenderCallback->OnVisibleChange(this);
	}

	void iRenderable::SetCoverageAmount(float afX)
	{
		if(mfCoverageAmount == afX) return;

		mfCoverageAmount = afX;

        //A little hack so that shadows are updated
		SetTransformUpdated(false);
	}

	void iRenderable::OnUpdateWorldTransform() {
	}

	const cVector3f& iRenderable::GetCalcScale()
	{
		cMatrixf *pModelMatrix = GetModelMatrix(NULL);

		if(pModelMatrix != NULL && mlCalcScaleMatrixCount != GetMatrixUpdateCount())
		{
			mlCalcScaleMatrixCount = GetMatrixUpdateCount();
			mvCalcScale.x = pModelMatrix->GetRight().Length();
			mvCalcScale.y = pModelMatrix->GetUp().Length();
			mvCalcScale.z = pModelMatrix->GetForward().Length();
		}

		return mvCalcScale;
	}

	// void iRenderable::bindCommandBuffer(ForgeRenderer& pipeline, ForgeCmdHandle& cmd) {
	// 	// pipeline.FrameIndex();
	// 	// cmdBinddescriptorSet(cmd, 0, m_cbObjectBuffer[frameIndex].m_descriptorSet);
	// }


	bool iRenderable::CollidesWithBV(cBoundingVolume *apBV)
	{
		return cMath::CheckBVIntersection(*GetBoundingVolume(), *apBV);
	}

	//-----------------------------------------------------------------------

	bool iRenderable::CollidesWithFrustum(cFrustum *apFrustum)
	{
		return apFrustum->CollideBoundingVolume(GetBoundingVolume()) != eCollision_Outside;
	}

}
