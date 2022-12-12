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

#include "scene/Beam.h"

#include "absl/types/span.h"
#include "graphics/Buffer.h"
#include "impl/tinyXML/tinyxml.h"

#include "math/Math.h"
#include "math/MathTypes.h"
#include "system/String.h"
#include "system/Platform.h"
#include <bx/debug.h>

#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/FileSearcher.h"

#include "graphics/Graphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/LowLevelGraphics.h"
#include <graphics/BufferHelper.h>

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/Scene.h"

#include "engine/Engine.h"
#include <array>
#include <cstdint>

namespace hpl {


    namespace entity::beam {

		struct VertexElement
		{
			float position[3];
			float normal[3];
			float tangent[4];
			float color[4];
			float texcoord[2];
		};
	}

	cBeam::cBeam(const tString asName, cResources *apResources,cGraphics *apGraphics) :
		iRenderable(asName)
	{
		mpMaterialManager = apResources->GetMaterialManager();
		mpFileSearcher = apResources->GetFileSearcher();
		mpLowLevelGraphics = apGraphics->GetLowLevel();

		msFileName = "";

		mvSize = 1;

		mbTileHeight = true;

		mColor = cColor(1,1,1,1);

		mpMaterial = NULL;

		mlLastRenderCount = -1;

		bgfx::VertexLayout layout;
        layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        auto vertexDef = BufferDefinition::CreateStructureBuffer(layout, 4);
        auto indexDef = BufferDefinition::CreateUint16IndexBuffer(6);
        m_vertexBuffer = std::move(Buffer(vertexDef));
        m_indexBuffer = std::move(Buffer(indexDef));

		// Buffer vtxData(entity::beam::VertexDefinition);
        // BufferIndex indexData(entity::beam::IndexDefinition);

		m_vertexBuffer.SetElementRange<entity::beam::VertexElement>(0, {
			{
				{
					{(mvSize.x/2),-(mvSize.y/2),0},
					{0, 0, 1},
					{0, 0, 0, 0},
					{1, 1, 1, 1},
					{1,1}
				},
				{
					{-(mvSize.x/2),-(mvSize.y/2),0},
					{0, 0, 1},
					{0, 0, 0, 0},
					{1, 1, 1, 1},
					{-1,1}
				},
				{
					{-(mvSize.x/2),(mvSize.y/2),0},
					{0, 0, 1},
					{0, 0, 0, 0},
					{1, 1, 1, 1},
					{-1,-1}
				},
				{
					{(mvSize.x/2),(mvSize.y/2),0},
					{0, 0, 1},
					{0, 0, 0, 0},
					{1, 1, 1, 1},
					{1,-1}
				}
			}
		});
		m_indexBuffer.SetElementRange<uint16_t>(0, {0, 1, 2, 2, 3, 0});
		
		std::array<Buffer*, 1> vertexBuffers = { &m_vertexBuffer };
        hpl::vertex::helper::UpdateTangentsFromPositionTexCoords(&m_indexBuffer, 
            absl::MakeSpan(vertexBuffers.begin(), vertexBuffers.end()), 4);

		mpEnd = hplNew( cBeamEnd, (asName + "_end",this));
		mpEnd->AddCallback(&mEndCallback);

		//Some temp setup
		mBoundingVolume.SetSize(cVector3f(mvSize.x, mvSize.y, mvSize.x));

		mbApplyTransformToBV = false;
	}

	//-----------------------------------------------------------------------

	cBeam::~cBeam()
	{
		hplDelete(mpEnd);
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		// if(mpVtxBuffer) hplDelete(mpVtxBuffer);
	}

	void cBeam::SetSize(const cVector2f& avSize)
	{
		mvSize = avSize;
		mBoundingVolume.SetSize(cVector3f(mvSize.x, mvSize.y, mvSize.x));

		SetTransformUpdated();
	}

	void cBeam::SetTileHeight(bool abX)
	{
		if(mbTileHeight == abX) return;

		mbTileHeight = abX;

		SetTransformUpdated();
	}

	void cBeam::SetMultiplyAlphaWithColor(bool abX)
	{
		if(mbMultiplyAlphaWithColor == abX) return;

		mbMultiplyAlphaWithColor = abX;
	}

	void cBeam::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;

		mColor = aColor;

		auto elements = m_vertexBuffer.GetElements<entity::beam::VertexElement>().subspan(2);
		// // auto bufferView = BufferView(m_vtxData, 0, 2);
		// auto elements = bufferView
		// 			.GetElements<entity::beam::VertexElement>();

		//Change "upper colors"
		if(mbMultiplyAlphaWithColor)
		{
			std::array<float, 4> arr = {mColor.r  * mColor.a, mColor.g  * mColor.a, mColor.b  * mColor.a, mColor.a};
			for(int i = 0; i < 2;++i)
			{
				std::copy(arr.begin(), arr.end(), elements[i].color);
			}
		}
		else
		{
			std::array<float, 4> arr = {mColor.r, mColor.g, mColor.b, mColor.a};
			for(int i = 0; i < 2; ++i)
			{
				std::copy(arr.begin(), arr.end(), elements[i].color);
			}
		}
		// bufferView.Update();
	}

	//-----------------------------------------------------------------------

	void cBeam::SetMaterial(cMaterial * apMaterial)
	{
		mpMaterial = apMaterial;
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* cBeam::GetBoundingVolume()
	{
		if(mbUpdateBoundingVolume)
		{
			cVector3f vMax = GetWorldPosition();
			cVector3f vMin = vMax;
			cVector3f vEnd = mpEnd->GetWorldPosition();

			if(vMax.x < vEnd.x) vMax.x = vEnd.x;
			if(vMax.y < vEnd.y) vMax.y = vEnd.y;
			if(vMax.z < vEnd.z) vMax.z = vEnd.z;

			if(vMin.x > vEnd.x) vMin.x = vEnd.x;
			if(vMin.y > vEnd.y) vMin.y = vEnd.y;
			if(vMin.z > vEnd.z) vMin.z = vEnd.z;

			vMin -= cVector3f(mvSize.x);
			vMax += cVector3f(mvSize.x);

			mBoundingVolume.SetLocalMinMax(vMin,vMax);

			mbUpdateBoundingVolume = false;
		}

		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void cBeam::UpdateGraphicsForFrame(float afFrameTime)
	{
		if(	mlStartTransformCount == GetTransformUpdateCount() &&
			mlEndTransformCount == GetTransformUpdateCount())
		{
			return;
		}

		////////////////////////////////
		//Get Axis
		mvAxis = mpEnd->GetWorldPosition() - GetWorldPosition();

		mvMidPosition =GetWorldPosition() + mvAxis*0.5f;
		float fDist = mvAxis.Length();
		mvAxis.Normalize();

		////////////////////////////////
		//Update vertex buffer
		cVector2f vBeamSize = cVector2f(mvSize.x, fDist);
	
		auto elements = m_vertexBuffer.GetElements<entity::beam::VertexElement>();
		auto texcoords = std::array<cVector2f, 4>({{
				{1,1},
				{0,1},
				{0, mbTileHeight ? -fDist/mvSize.y : 0},
				{1, mbTileHeight ? -fDist/mvSize.y : 0},
			}});
		std::array<cVector3f, 4> positions = {cVector3f((vBeamSize.x/2),-(vBeamSize.y/2),0),
								cVector3f(-(vBeamSize.x/2),-(vBeamSize.y/2),0),
								cVector3f(-(vBeamSize.x/2),(vBeamSize.y/2),0),
								cVector3f((vBeamSize.x/2),(vBeamSize.y/2),0)};

		BX_ASSERT(elements.size() == positions.size(), "Invalid number of elements");
		BX_ASSERT(elements.size() == texcoords.size(), "Invalid number of elements");
	    static_assert(sizeof(elements[0].position) == sizeof(positions[0].v), "Size mismatch" );
		static_assert(sizeof(elements[0].texcoord) == sizeof(texcoords[0].v), "Size mismatch" );
		for(size_t i = 0; i < elements.size(); i++) {
			std::copy(std::begin(positions[i].v), std::end(positions[i].v), elements[i].position);
			std::copy(std::begin(texcoords[i].v), std::end(texcoords[i].v), elements[i].texcoord);
		}
		// m_vtxData.Update();

	}

	//-----------------------------------------------------------------------

	cMatrixf* cBeam::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();

		m_mtxTempTransform = GetWorldMatrix();
		cVector3f vForward, vRight, vUp;

		cVector3f vCameraForward = apFrustum->GetOrigin() - GetWorldPosition();
		vCameraForward.Normalize();

		vUp = mvAxis;//cMath::MatrixMul(GetWorldMatrix().GetRotation(),mvAxis);
		//vUp.Normalize();

		if(vUp == vForward)
		{
			vRight = cMath::Vector3Cross(vUp, vCameraForward);
			Warning("Beam Right vector is not correct! Contact programmer!\n");
		}
		else
			vRight = cMath::Vector3Cross(vUp, vCameraForward);

		vRight.Normalize();
		vForward = cMath::Vector3Cross(vRight, vUp);

		//Set right vector
		m_mtxTempTransform.m[0][0] = vRight.x;
		m_mtxTempTransform.m[1][0] = vRight.y;
		m_mtxTempTransform.m[2][0] = vRight.z;

		//Set up vector
		m_mtxTempTransform.m[0][1] = vUp.x;
		m_mtxTempTransform.m[1][1] = vUp.y;
		m_mtxTempTransform.m[2][1] = vUp.z;

		//Set forward vector
		m_mtxTempTransform.m[0][2] = vForward.x;
		m_mtxTempTransform.m[1][2] = vForward.y;
		m_mtxTempTransform.m[2][2] = vForward.z;

		m_mtxTempTransform.SetTranslation(mvMidPosition);

		return &m_mtxTempTransform;
	}

	//-----------------------------------------------------------------------

	int cBeam::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	//-----------------------------------------------------------------------


	bool cBeam::LoadXMLProperties(const tString asFile)
	{
		msFileName = asFile;

		tString sNewFile = cString::SetFileExt(asFile,"beam");
		tWString sPath = mpFileSearcher->GetFilePath(sNewFile);
		if(sPath != _W(""))
		{
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile==NULL) return false;
			TiXmlDocument *pDoc = hplNew( TiXmlDocument, () );
			if(pDoc->LoadFile(pFile))
			{
				TiXmlElement *pRootElem = pDoc->RootElement();

				TiXmlElement *pMainElem = pRootElem->FirstChildElement("MAIN");
				if(pMainElem!=NULL)
				{
					tString sMaterial = cString::ToString(pMainElem->Attribute("Material"),"");
					cVector2f vSize = cString::ToVector2f(pMainElem->Attribute("Size"),1);

					bool bTileHeight = cString::ToBool(pMainElem->Attribute("TileHeight"),true);
					bool bMultiplyAlphaWithColor = cString::ToBool(pMainElem->Attribute("MultiplyAlphaWithColor"),false);

					cColor StartColor = cString::ToColor(pMainElem->Attribute("StartColor"),cColor(1,1));
					cColor EndColor = cString::ToColor(pMainElem->Attribute("EndColor"),cColor(1,1));

					SetSize(vSize);
					SetTileHeight(bTileHeight);
					SetMultiplyAlphaWithColor(bMultiplyAlphaWithColor);
					SetColor(StartColor);
					mpEnd->SetColor(EndColor);

					/////////////////
					//Load material
					cMaterial *pMat = mpMaterialManager->CreateMaterial(sMaterial);
					if(pMat)	{
						SetMaterial(pMat);
					}
					else{
						Error("Couldn't load material '%s' in Beam file '%s'",
											sMaterial.c_str(), sNewFile.c_str());
						return false;
					}
				}
				else
				{
					Error("Cannot find main element in %s\n",sNewFile.c_str());
					return false;
				}
			}
			else
			{
				Error("Couldn't load file '%s'\n",sNewFile.c_str());
			}
			if(pFile) fclose(pFile);
			hplDelete(pDoc);
		}
		else
		{
			Error("Couldn't find file '%s'\n",sNewFile.c_str());
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cBeam::IsVisible()
	{
		if(mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0) return false;

		return mbIsVisible;
	}

	bool cBeam::Submit(LayoutStream& input, GraphicsContext& context) {
		return false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// BEAM END TRANSFORM UPDATE CALLBACK
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cBeamEnd_UpdateCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		cBeamEnd *pEnd = static_cast<cBeamEnd*>(apEntity);

		pEnd->mpBeam->SetTransformUpdated(true);
	}

	void cBeamEnd::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) {
			return;
		}

		mColor = aColor;

		auto elements = 
			mpBeam->m_vertexBuffer.GetElements<entity::beam::VertexElement>()
				.subspan(2);

		// auto bufferView = BufferView(mpBeam->m_vtxData, 2, 2);
		// auto elements = bufferView
		// 			.GetElements<entity::beam::VertexElement>();

		//Change "upper colors"
		if(mpBeam->mbMultiplyAlphaWithColor)
		{
			std::array<float, 4> arr = {mColor.r  * mColor.a, mColor.g  * mColor.a, mColor.b  * mColor.a, mColor.a};
			for(int i = 0; i < 2;++i)
			{
				std::copy(arr.begin(), arr.end(), elements[i].color);
			}
		}
		else
		{
			std::array<float, 4> arr = {mColor.r, mColor.g, mColor.b, mColor.a};
			for(int i = 0; i < 2; ++i)
			{
				std::copy(arr.begin(), arr.end(), elements[i].color);
			}
		}
		// bufferView.Update();

	}

}
