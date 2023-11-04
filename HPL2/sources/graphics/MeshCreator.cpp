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

#include "graphics/MeshCreator.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "resources/Resources.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "resources/MaterialManager.h"
#include "resources/AnimationManager.h"
#include "math/Math.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMeshCreator::cMeshCreator(iLowLevelGraphics *apLowLevelGraphics, cResources *apResources)
	{
		mpLowLevelGraphics = apLowLevelGraphics;
		mpResources = apResources;
	}

	//-----------------------------------------------------------------------

	cMeshCreator::~cMeshCreator()
	{
	}

	iVertexBuffer* cMeshCreator::CreateWireframeVertexBuffer(iVertexBuffer *apSrc)
	{
		iVertexBuffer *pDest = apSrc->CreateCopy(eVertexBufferType_Hardware, eVertexBufferUsageType_Static, apSrc->GetVertexElementFlags());

        pDest->ResizeIndices(apSrc->GetIndexNum()*4);
		unsigned int* pDestArray = pDest->GetIndices();
		int lDestCount=0;

		unsigned int* pIdxArray = apSrc->GetIndices();
		for(int tri=0; tri < apSrc->GetIndexNum(); tri+=3)
		{
			unsigned int* pTri = &pIdxArray[tri];

			for(int i=0; i<3; ++i)
			{
				int lNext = i+1 >=3 ? 0 : i+1;
				pDestArray[lDestCount] = pTri[i];
				pDestArray[lDestCount+1] = pTri[lNext];
                lDestCount+=2;
			}
		}

		pDest->UpdateData(0, true);

		return pDest;
	}

	cMesh* cMeshCreator::CreateCone(const tString &asName, const cVector2f &avSize, int alSections, const tString &asMaterial)
	{
		//////////////////////////////////////////
		// Create Vertex Buffer
		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	eVertexBufferUsageType_Static);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		////////////////////////
		// Set up variables
		float fAngleStep = k2Pif/(float)alSections;
		float fHalfHeight = avSize.y*0.5f;
		cColor col = cColor(1,1);

		/////////////////////////
		// Create apex vertex
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(0,fHalfHeight,0));
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(0,1,0));
		pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, col);
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0));

		///////////////////////////////////////////
		// Create vertices for base
		tVector3fVec vVertices;
		CreateCircumference(avSize.x, fAngleStep, -fHalfHeight, vVertices);

		for(int i=0;i<(int)vVertices.size();++i)
		{
			const cVector3f& vVertex = vVertices[i];
			cVector3f vNormal = vVertex;
			vNormal.Normalize();

			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vVertex);
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal, vNormal);
			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, col);
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0));
		}

		// Base center vertex
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(0,-fHalfHeight,0));
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(0,-1,0));
		pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, col);
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0));

		////////////////////////////
		// Triangles for sides
		{
			WrapUpperCap(pVtxBuffer, 0, 1, alSections);
		}

		////////////////////////////
		// Triangles for base
		{
			int lLastVertex = pVtxBuffer->GetVertexNum()-1;
			int lCapStart = lLastVertex-alSections;
			WrapLowerCap(pVtxBuffer, lLastVertex, lCapStart, alSections);
		}

		if(!pVtxBuffer->Compile(eVertexCompileFlag_CreateTangents))
		{
			hplDelete(pVtxBuffer);
			pVtxBuffer = NULL;
		}

		cMesh* pMesh = hplNew( cMesh, (asName, _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()));
		cSubMesh* pSubMesh = pMesh->CreateSubMesh("Main");

		cMaterial *pMat = mpResources->GetMaterialManager()->CreateMaterial(asMaterial);
		pSubMesh->SetMaterial(pMat);
		pSubMesh->SetVertexBuffer(pVtxBuffer);

		return pMesh;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshCreator::CreateSkyBoxVertexBuffer(float afSize)
	{
		iVertexBuffer* pSkyBox = mpLowLevelGraphics->CreateVertexBuffer(
										eVertexBufferType_Hardware,
										eVertexBufferDrawType_Quad,eVertexBufferUsageType_Static);
		pSkyBox->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pSkyBox->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		pSkyBox->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);


		float fSize = afSize;
		for(int x=-1; x<=1;x++)
			for(int y=-1; y<=1;y++)
				for(int z=-1; z<=1;z++)
				{
					if(x==0 && y==0 && z==0)continue;
					if(std::abs(x) + std::abs(y) + std::abs(z) > 1)continue;

					//Direction (could say inverse normal) of the quad.
					cVector3f vDir(0);
					cVector3f vSide(0);

					cVector3f vAdd[4];
					if(std::abs(x)){
						vDir.x = (float)x;

						vAdd[0].y = 1;	vAdd[0].z = 1;	vAdd[0].x =0;
						vAdd[1].y = -1;	vAdd[1].z = 1;	vAdd[1].x =0;
						vAdd[2].y = -1;	vAdd[2].z = -1;	vAdd[2].x =0;
						vAdd[3].y = 1;	vAdd[3].z = -1;	vAdd[3].x =0;
					}
					else if(std::abs(y)){
						vDir.y = (float)y;

						vAdd[0].z = 1;	vAdd[0].x = 1;	vAdd[0].y =0;
						vAdd[1].z = -1;	vAdd[1].x = 1;	vAdd[1].y =0;
						vAdd[2].z = -1;	vAdd[2].x = -1;	vAdd[2].y =0;
						vAdd[3].z = 1;	vAdd[3].x = -1;	vAdd[3].y =0;
					}
					else if(std::abs(z)){
						vAdd[0].y = 1;	vAdd[0].x = 1;	vAdd[0].z =0;
						vAdd[1].y = 1;	vAdd[1].x = -1;	vAdd[1].z =0;
						vAdd[2].y = -1;	vAdd[2].x = -1;	vAdd[2].z =0;
						vAdd[3].y = -1;	vAdd[3].x = 1;	vAdd[3].z =0;

						vDir.z = (float)z;
					}


					//Log("Side: (%.0f : %.0f : %.0f) [ ", vDir.x,  vDir.y,vDir.z);
					for(int i=0;i<4;i++)
					{
						int idx = i;
						if(x + y + z < 0) idx = 3-i;

						pSkyBox->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1,1,1));
						pSkyBox->AddVertexVec3f(eVertexBufferElement_Position, (vDir+vAdd[idx])*fSize);
						pSkyBox->AddVertexVec3f(eVertexBufferElement_Texture0, vDir+vAdd[idx]);

						vSide = vDir+vAdd[idx];
						//Log("%d: (%.1f : %.1f : %.1f) ", i,vSide.x,  vSide.y,vSide.z);
					}
					//Log("\n");
				}

		for(int i=0;i<24;i++) pSkyBox->AddIndex(i);

		if(!pSkyBox->Compile(0))
		{
			hplDelete(pSkyBox);
			return NULL;
		}
		return pSkyBox;
	}

	cVector3f cMeshCreator::GetBoxTex(int i,int x, int y, int z, cVector3f *vAdd)
	{
		cVector3f vTex;

		if(std::abs(x)){
			vTex.x = vAdd[i].z;
			vTex.y = vAdd[i].y;
		}
		else if(std::abs(y)){
			vTex.x = vAdd[i].x;
			vTex.y = vAdd[i].z;
		}
		else if(std::abs(z)){
			vTex.x = vAdd[i].x;
			vTex.y = vAdd[i].y;
		}

		//Inverse for negative directions
		if(x+y+z <0)
		{
			vTex.x = -vTex.x;
			vTex.y = -vTex.y;
		}

		return vTex;
	}
	int cMeshCreator::GetBoxIdx(int i,int x, int y, int z)
	{
		int idx = i;
		if(x + y + z > 0) idx = 3-i;

		return idx;
	}

	//-----------------------------------------------------------------------

	void cMeshCreator::CreateCircumference(float afRadius, float afAngleStep, float afHeight, tVector3fVec& avVertices)
	{
		float fEnd = k2Pif-afAngleStep;
		for(float fAngle=0; fAngle<=fEnd; fAngle+=afAngleStep)
		{
			cVector3f vPoint = cVector3f(cMath::RoundFloatToDecimals(afRadius*cos(fAngle), 6),
										 cMath::RoundFloatToDecimals(afHeight, 6),
										 cMath::RoundFloatToDecimals(afRadius*sin(fAngle),6));
			avVertices.push_back(vPoint);
		}
	}

	//-----------------------------------------------------------------------

	void cMeshCreator::WrapSides(iVertexBuffer* apVtxBuffer, int alStartVertexIdx,int alSections)
	{
		/////////////////////////////////////////////////
		// Create indices  like this		0 --- 1 --- 2 --- ... --- 0
		//									|   / |   / |
		//									|  /  |  /  |
		//									| /	  | /   |
		//									Se---Se+1---Se+2---2S-1--- S


		for(int i=0;i<alSections-1;++i)
		{
			int lPoint0 = alStartVertexIdx+i;
			int lPoint1 = lPoint0+1;
			int lPoint2 = lPoint0+alSections;
			int lPoint3 = lPoint2+1;

			apVtxBuffer->AddIndex(lPoint0);
			apVtxBuffer->AddIndex(lPoint2);
			apVtxBuffer->AddIndex(lPoint1);

			apVtxBuffer->AddIndex(lPoint1);
			apVtxBuffer->AddIndex(lPoint2);
			apVtxBuffer->AddIndex(lPoint3);
		}

		{
			int lPoint0 = alStartVertexIdx+alSections-1;
			int lPoint1 = alStartVertexIdx;
			int lPoint2 = lPoint0+alSections;
			int lPoint3 = lPoint0+1;

			apVtxBuffer->AddIndex(lPoint0);
			apVtxBuffer->AddIndex(lPoint2);
			apVtxBuffer->AddIndex(lPoint1);

			apVtxBuffer->AddIndex(lPoint1);
			apVtxBuffer->AddIndex(lPoint2);
			apVtxBuffer->AddIndex(lPoint3);

			//Log("%d %d %d\t %d %d %d\n", lPoint0, lPoint2, lPoint1,
			//						lPoint1, lPoint2, lPoint3);
		}
	}

	void cMeshCreator::WrapUpperCap(iVertexBuffer* apVtxBuffer, int alCenterVertexIdx, int alStartVertexIdx, int alSections)
	{
		for(int i=0;i<alSections-1;++i)
		{
			int lBase = alStartVertexIdx+i;
			apVtxBuffer->AddIndex(alCenterVertexIdx);
			apVtxBuffer->AddIndex(lBase);
			apVtxBuffer->AddIndex(lBase+1);
		}
		apVtxBuffer->AddIndex(alCenterVertexIdx);
		apVtxBuffer->AddIndex(alStartVertexIdx+alSections-1);
		apVtxBuffer->AddIndex(alStartVertexIdx);
	}

	void cMeshCreator::WrapLowerCap(iVertexBuffer* apVtxBuffer, int alCenterVertexIdx, int alStartVertexIdx, int alSections)
	{
		for(int i=0;i<alSections-1;++i)
		{
			int lBase = alStartVertexIdx+i;
			apVtxBuffer->AddIndex(alCenterVertexIdx);
			apVtxBuffer->AddIndex(lBase+1);
			apVtxBuffer->AddIndex(lBase);
		}

		apVtxBuffer->AddIndex(alCenterVertexIdx);
		apVtxBuffer->AddIndex(alStartVertexIdx);
		apVtxBuffer->AddIndex(alStartVertexIdx+alSections-1);
	}


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

}
