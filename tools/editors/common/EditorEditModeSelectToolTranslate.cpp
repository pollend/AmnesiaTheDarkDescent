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

#include "EditorEditModeSelectToolTranslate.h"

#include "EditorWindowViewport.h"
#include "EditorEditModeSelect.h"

#include "EditorGrid.h"

#include "EditorActionSelection.h"
#include "EditorSelection.h"

#include "graphics/DebugDraw.h"

cEditorEditModeSelectToolTranslate::cEditorEditModeSelectToolTranslate(cEditorEditModeSelect* apParent, cEditorSelection* apSelection) : cEditorEditModeSelectTool(eSelectToolMode_Translate,apParent, apSelection)
{
}


bool cEditorEditModeSelectToolTranslate::IsActive()
{
	return mpSelection->CanTranslate();
}

bool cEditorEditModeSelectToolTranslate::CheckRayIntersectsAxis(eSelectToolAxis aeAxis, const cVector3f& avRayStart, const cVector3f& avRayEnd)
{
	cVector3f vMin = mvAxisMin[aeAxis];
	cVector3f vMax = mvAxisMax[aeAxis];

	return cMath::CheckAABBLineIntersection(vMin,vMax,avRayStart,avRayEnd, NULL, NULL);
}


void cEditorEditModeSelectToolTranslate::CheckMouseOverAxis()
{
	cVector3f vAxisIntersect[3];
	cVector3f vHeadIntersect[3];
	cVector3f vPlaneIntersect[3];

	bool bRayIntersectedAxis[3];
	bool bRayIntersectedPlane[3];

	float fMinDist = 999999999.0f;

	for(int i=eSelectToolAxis_X;i<eSelectToolAxis_LastEnum;++i)
	{
		vAxisIntersect[i] = 99999999999.0f;
		vHeadIntersect[i] = 99999999999.0f;
		vPlaneIntersect[i] = 99999999999.0f;

		bRayIntersectedAxis[i] = cMath::CheckAABBLineIntersection(mvAxisMin[i],
															  mvAxisMax[i],
															  mvTransformedRayStart,
															  mvTransformedRayEnd,
															  &vAxisIntersect[i],
															  NULL) ||
							 cMath::CheckAABBLineIntersection(mvHeadMin[i],
															  mvHeadMax[i],
															  mvTransformedRayStart,
															  mvTransformedRayEnd,
															  &vHeadIntersect[i],
															  NULL);

		bRayIntersectedPlane[i] = cMath::CheckAABBLineIntersection(mvAxesPlaneBoxMin,
																   mvAxesPlaneBoxMax[i],
																   mvTransformedRayStart,
																   mvTransformedRayEnd,
																   &vPlaneIntersect[i],
																   NULL);


		//////////////////////////////////////////////////////
		// Check closest intersection
		{
			float fDistToAxis;
			float fDistToHead;
			fDistToAxis = cMath::Vector3DistSqr(mvTransformedRayStart, vAxisIntersect[i]);
			fDistToHead = cMath::Vector3DistSqr(mvTransformedRayStart, vHeadIntersect[i]);
			if(fDistToAxis < fMinDist)
			{
				fMinDist = fDistToAxis;
			}
			else if(fDistToHead < fMinDist)
			{
				fMinDist = fDistToHead;
			}
			else
				bRayIntersectedAxis[i] = false;

			float fDistToPlane = cMath::Vector3DistSqr(mvTransformedRayStart, vPlaneIntersect[i]);
			if(fDistToPlane < fMinDist)
			{
				fMinDist = fDistToPlane;
			}
			else
				bRayIntersectedPlane[i] = false;
		}
	}

	mvAxisMouseOver[eSelectToolAxis_X] = bRayIntersectedAxis[eSelectToolAxis_X] || bRayIntersectedPlane[0] || bRayIntersectedPlane[2];
	mvAxisMouseOver[eSelectToolAxis_Y] = bRayIntersectedAxis[eSelectToolAxis_Y] || bRayIntersectedPlane[0] || bRayIntersectedPlane[1];
	mvAxisMouseOver[eSelectToolAxis_Z] = bRayIntersectedAxis[eSelectToolAxis_Z] || bRayIntersectedPlane[1] || bRayIntersectedPlane[2];
}

//----------------------------------------------------------------

cMatrixf& cEditorEditModeSelectToolTranslate::GetTransformMatrix()
{
	//if(mpSelection->mbTransformed)
	//{
	mmtxTransformMatrix = cMath::MatrixTranslate(mpSelection->GetCenterTranslation()*-1);
	//	mpSelection->mbTransformed = false;
	//}

	return mmtxTransformMatrix;
}

//----------------------------------------------------------------

//----------------------------------------------------------------

void cEditorEditModeSelectToolTranslate::UpdateTransformation()
{
	iEditorBase* pEditor = mpEditMode->GetEditor();

	if(pEditor->GetFlags(eEditorFlag_MouseMoved)==false)
		return;

	cEditorWindowViewport* pViewport = pEditor->GetFocusedViewport();

	bool bIntersect = cMath::CheckPlaneLineIntersection(mEditingPlane, mvRayStart, mvRayEnd, &mvEditVectorEnd,NULL);

	if(bIntersect)
	{
		cVector3f vDisplacement = mvEditVectorEnd - mvEditVectorStart;

		for(int i=eSelectToolAxis_X; i<eSelectToolAxis_LastEnum; ++i)
		{
			if(mvAxisSelected[i]==false)
				vDisplacement.v[i] = 0;
		}

		mvEditVectorEnd = mvEditVectorStart + vDisplacement;

		mpSelection->SetRelativeTranslation(vDisplacement, cEditorGrid::GetSnapToGrid());

		mpEditMode->GetEditor()->SetLayoutNeedsUpdate(true);
	}
}

//----------------------------------------------------------------

void cEditorEditModeSelectToolTranslate::UpdateToolBoundingVolume()
{
	cEditorWindowViewport *pViewport = mpEditMode->GetEditor()->GetFocusedViewport();
	cCamera* pCam = pViewport->GetCamera();

	cVector3f vViewpos = cMath::MatrixMul(pCam->GetViewMatrix(), mpSelection->GetCenterTranslation());

	if(pViewport->GetCamera()->GetProjectionType()==eProjectionType_Orthographic)
	{
		mfUsedAxisLength = pViewport->GetCamera()->GetOrthoViewSize().x*0.2f;
	}
	else
	{
		mfUsedAxisLength =  cMath::Abs(vViewpos.z)/mfAxisBaseLength;
	}

	float fAxisHalfWidth = mfUsedAxisLength*0.04f;
	float fAxisWidth = fAxisHalfWidth*2;
	float fAxisLength = mfUsedAxisLength*0.8f;

	float fHeadHalfWidth = mfUsedAxisLength*0.07f;
	float fHeadWidth = fHeadHalfWidth*2;
	float fHeadLength = mfUsedAxisLength*0.2f;

	mvAxisMin[eSelectToolAxis_X] = cVector3f(0,-fAxisHalfWidth,-fAxisHalfWidth);
	mvAxisMin[eSelectToolAxis_Y] = cVector3f(-fAxisHalfWidth,0, -fAxisHalfWidth);
	mvAxisMin[eSelectToolAxis_Z] = cVector3f(-fAxisHalfWidth,-fAxisHalfWidth,0);

	mvAxisMax[eSelectToolAxis_X] = mvAxisMin[eSelectToolAxis_X] + cVector3f(fAxisLength,fAxisWidth,fAxisWidth);
	mvAxisMax[eSelectToolAxis_Y] = mvAxisMin[eSelectToolAxis_Y] + cVector3f(fAxisWidth,fAxisLength,fAxisWidth);
	mvAxisMax[eSelectToolAxis_Z] = mvAxisMin[eSelectToolAxis_Z] + cVector3f(fAxisWidth,fAxisWidth,fAxisLength);

	mvHeadMin[eSelectToolAxis_X] = cVector3f(fAxisLength, -fHeadHalfWidth, -fHeadHalfWidth);
	mvHeadMin[eSelectToolAxis_Y] = cVector3f(-fHeadHalfWidth, fAxisLength, -fHeadHalfWidth);
	mvHeadMin[eSelectToolAxis_Z] = cVector3f(-fHeadHalfWidth, -fHeadHalfWidth, fAxisLength);

	mvHeadMax[eSelectToolAxis_X] = mvHeadMin[eSelectToolAxis_X] + cVector3f(fHeadLength,fHeadWidth,fHeadWidth);
	mvHeadMax[eSelectToolAxis_Y] = mvHeadMin[eSelectToolAxis_Y] + cVector3f(fHeadWidth,fHeadLength,fHeadWidth);
	mvHeadMax[eSelectToolAxis_Z] = mvHeadMin[eSelectToolAxis_Z] + cVector3f(fHeadWidth,fHeadWidth,fHeadLength);


	mvAxesPlaneBoxMin = 0;
	mvAxesPlaneBoxMax[0] = cVector3f(mfUsedAxisLength,mfUsedAxisLength,0) * 0.2f;
	mvAxesPlaneBoxMax[1] = cVector3f(0,mfUsedAxisLength, mfUsedAxisLength) * 0.2f;
	mvAxesPlaneBoxMax[2] = cVector3f(mfUsedAxisLength,0,mfUsedAxisLength) * 0.2f;
}

//----------------------------------------------------------------

void cEditorEditModeSelectToolTranslate::DrawAxes(cEditorWindowViewport* apViewport, DebugDraw *apFunctions, float afAxisLength)
{
	cVector3f vAxes[3];
	cColor col[3];

	float fX = 0.2f*afAxisLength;
	float fYZ = 0.05f*afAxisLength;

	cMatrixf mtxTransform = cMath::MatrixTranslate(mpSelection->GetCenterTranslation());
	DebugDraw::DebugDrawOptions options;
	options.m_transform = cMath::ToForgeMatrix4(mtxTransform);
	options.m_depthTest = DebugDraw::DebugDepthTest::Always;

	for(int i=eSelectToolAxis_X; i<eSelectToolAxis_LastEnum; ++i)
	{
		col[i] = mvAxisSelected[i] ? mColorSelected : (mvAxisMouseOver[i] ? mColorMouseOver : mvAxisColor[i]);
		vAxes[i] = 0;
		vAxes[i].v[i] = afAxisLength;

	     apFunctions->DebugDrawLine(Vector3(0), cMath::ToForgeVec3(vAxes[i]), cMath::ToForgeVec4(col[i]), options);

		// DEBUG: Draw axes bounding boxes
		//apFunctions->GetLowLevelGfx()->DrawBoxMinMax(mvAxisMin[i], mvAxisMax[i], cColor(1,1));
		//apFunctions->GetLowLevelGfx()->DrawBoxMinMax(mvHeadMin[i], mvHeadMax[i], cColor(1,1));
	}

	const auto drawArrowGeometry = [&](const cColor& c, const DebugDraw::DebugDrawOptions& options) {
		apFunctions->DrawQuad(
			Vector3(0,0,0),
			Vector3(-fX,fYZ,fYZ),
			Vector3(0,0,0),
			Vector3(-fX,-fYZ,fYZ),
			cMath::ToForgeVec4(c),options
		);
		apFunctions->DrawQuad(
			Vector3(0,0,0),
			Vector3(-fX,-fYZ,-fYZ),
			Vector3(0,0,0),
			Vector3(-fX,fYZ,-fYZ),
			cMath::ToForgeVec4(c),options
		);
		apFunctions->DrawQuad(
			Vector3(0,0,0),
			Vector3(-fX,fYZ,-fYZ),
			Vector3(0,0,0),
			Vector3(-fX,fYZ,fYZ),
			cMath::ToForgeVec4(c),options
		);
		apFunctions->DrawQuad(
			Vector3(0,0,0),
			Vector3(-fX,-fYZ,fYZ),
			Vector3(0,0,0),
			Vector3(-fX,-fYZ,-fYZ),
			cMath::ToForgeVec4(c),options
		);
	};
	DebugDraw::DebugDrawOptions oo;
	drawArrowGeometry(col[0], oo);

	apFunctions->DebugDrawBoxMinMax(Vector3(0), cMath::ToForgeVec3(vAxes[0]+vAxes[1])*.2f, cMath::ToForgeVec4(col[0] + col[1]), options);
	apFunctions->DebugDrawBoxMinMax(Vector3(0), cMath::ToForgeVec3(vAxes[0]+vAxes[2])*.2f, cMath::ToForgeVec4(col[0] + col[2]), options);
	apFunctions->DebugDrawBoxMinMax(Vector3(0), cMath::ToForgeVec3(vAxes[1]+vAxes[2])*.2f, cMath::ToForgeVec4(col[1] + col[2]), options);

	cMatrixf mtxXAxisHeadTransform = cMath::MatrixMul(cMath::MatrixTranslate(cVector3f(afAxisLength,0,0)),mtxTransform);
	cMatrixf mtxYAxisHeadTransform = cMath::MatrixMul(cMath::MatrixTranslate(cVector3f(0,afAxisLength,0)),
													  cMath::MatrixMul(mtxTransform, cMath::MatrixRotate(cVector3f(0,0,kPi2f),eEulerRotationOrder_XYZ)));
	cMatrixf mtxZAxisHeadTransform = cMath::MatrixMul(cMath::MatrixTranslate(cVector3f(0,0,afAxisLength)),
													  cMath::MatrixMul(mtxTransform, cMath::MatrixRotate(cVector3f(0,-kPi2f,0),eEulerRotationOrder_XYZ)));

	options.m_transform = cMath::ToForgeMatrix4(mtxXAxisHeadTransform);
	drawArrowGeometry(col[0], options);

	options.m_transform = cMath::ToForgeMatrix4(mtxYAxisHeadTransform);
	drawArrowGeometry(col[1], options);

	options.m_transform = cMath::ToForgeMatrix4(mtxZAxisHeadTransform);
	drawArrowGeometry(col[2], options);
}

//-----------------------------------------------------------------------------------

iEditorAction* cEditorEditModeSelectToolTranslate::CreateAction()
{
	bool bUseSnap = mpEditMode->GetEditor()->GetGrid()->GetSnapToGrid();
	cVector3f vDisplacement = mvEditVectorEnd - mvEditVectorStart;

	mvEditVectorStart = 0;
	mvEditVectorEnd = 0;

	if(vDisplacement==0)
		return NULL;

	return hplNew(cEditorActionSelectionTranslate,(mpEditMode, vDisplacement, bUseSnap) );
}

//-----------------------------------------------------------------------------------
