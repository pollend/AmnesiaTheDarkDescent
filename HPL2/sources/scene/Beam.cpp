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

#include "impl/LegacyVertexBuffer.h"
#include "impl/tinyXML/tinyxml.h"

#include "math/Math.h"
#include "system/String.h"
#include "system/Platform.h"

#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/FileSearcher.h"

#include "graphics/Graphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/GraphicsBuffer.h"
#include "graphics/GeometrySet.h"
#include "graphics/MeshUtility.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/Scene.h"

#include "engine/Engine.h"

#include "Common_3/Utilities/Math/MathTypes.h"

namespace hpl {
    const uint32_t BeamNumberVerts = 4;

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

        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
        m_geometry = opaqueSet.allocate(4 * 2, 6);

        std::array<float3, BeamNumberVerts> positionCoords = { float3((mvSize.x / 2), -(mvSize.y / 2), 0.0f),
                              float3(-(mvSize.x / 2), -(mvSize.y / 2), 0.0f),
                              float3(-(mvSize.x / 2), (mvSize.y / 2), 0.0f),
                              float3((mvSize.x / 2), (mvSize.y / 2), 0.0f) };

        std::array<float2, BeamNumberVerts> texcoords = { (float2(1.0f, 1.0f) + float2(1,1)) / 2.0f, // Bottom left
                                                          (float2(-1.0f, 1.0f) + float2(1, 1)) / 2.0f, // Bottom right
                                                          (float2(-1.0f, -1.0f) + float2(1, 1)) / 2.0f, // Top left
                                                          (float2(1.0f, -1.0f) + float2(1, 1)) / 2.0f }; // Top right
        {
            auto& indexStream = m_geometry->indexBuffer();
            auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto normalStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
            auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
            auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);
            auto tangentStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);

            BufferUpdateDesc indexUpdateDesc = { indexStream.m_handle, m_geometry->indexOffset() * GeometrySet::IndexBufferStride, GeometrySet::IndexBufferStride * 6 };
            BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle, m_geometry->vertexOffset() * positionStream->stride(), positionStream->stride() * BeamNumberVerts * 2 };
            BufferUpdateDesc normalUpdateDesc = { normalStream->buffer().m_handle, m_geometry->vertexOffset() * normalStream->stride(), normalStream->stride() * BeamNumberVerts  * 2};
            BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle, m_geometry->vertexOffset() * colorStream->stride(), colorStream->stride() * BeamNumberVerts  * 2};
            BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle, m_geometry->vertexOffset() * textureStream->stride(), textureStream->stride() * BeamNumberVerts * 2};
            BufferUpdateDesc tangentUpdateDesc = { tangentStream->buffer().m_handle, m_geometry->vertexOffset() * tangentStream->stride(), tangentStream->stride() * BeamNumberVerts * 2};

            beginUpdateResource(&indexUpdateDesc);
            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&normalUpdateDesc);
            beginUpdateResource(&colorUpdateDesc);
            beginUpdateResource(&textureUpdateDesc);
            beginUpdateResource(&tangentUpdateDesc);

            GraphicsBuffer gpuIndexBuffer(indexUpdateDesc);
            GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
            GraphicsBuffer gpuNormalBuffer(normalUpdateDesc);
            GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
            GraphicsBuffer gpuTextureBuffer(textureUpdateDesc);
            GraphicsBuffer gpuTangentBuffer(tangentUpdateDesc);

            auto indexView = gpuIndexBuffer.CreateIndexView();
            {
                uint32_t index = 0;
                for (int i = 0; i < 3; i++) {
                    indexView.Write(index++, i);
                }
                for (int i = 2; i < 5; i++) {
                    indexView.Write(index++, i == 4 ? 0 : i);
                }
            }
            for(size_t copyIdx  = 0; copyIdx < 2; copyIdx++) {
                auto positionView = gpuPositionBuffer.CreateStructuredView<float3>(positionStream->stride() * copyIdx * 4) ;
                auto normalView = gpuNormalBuffer.CreateStructuredView<float3>(normalStream->stride() * copyIdx * 4);
                auto colorView = gpuColorBuffer.CreateStructuredView<float4>(colorStream->stride() * copyIdx * 4);
                auto textureView = gpuTextureBuffer.CreateStructuredView<float2>(textureStream->stride() * copyIdx * 4);
                auto tangentView = gpuTangentBuffer.CreateStructuredView<float3>(tangentStream->stride() * copyIdx * 4);
                for (size_t i = 0; i < 4; i++) {
                    positionView.Write(i, positionCoords[i]);
                    normalView.Write(i, float3(0.0f, 0.0f, 1.0f));
                    colorView.Write(i, float4(1,1,1,1));
                    textureView.Write(i, texcoords[i]);
                }
                Matrix4 trans = Matrix4::identity();
                hpl::MeshUtility::MikkTSpaceGenerate(4, 6, &indexView, &positionView, &textureView, &normalView, &tangentView);
            }

            endUpdateResource(&indexUpdateDesc);
            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&normalUpdateDesc);
            endUpdateResource(&colorUpdateDesc);
            endUpdateResource(&textureUpdateDesc);
            endUpdateResource(&tangentUpdateDesc);
        }

		//mpVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

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
	}

    DrawPacket cBeam::ResolveDrawPacket(const ForgeRenderer::Frame& frame) {

        DrawPacket packet;

        DrawPacket::GeometrySetBinding binding{};
        packet.m_type = DrawPacket::DrawGeometryset;
        binding.m_subAllocation = m_geometry.get();
        binding.m_indexOffset = 0;
        binding.m_set = GraphicsAllocator::AllocationSet::OpaqueSet;
        binding.m_numIndices = 6;
        binding.m_vertexOffset = (m_activeCopy * 4);
        packet.m_unified = binding;
        return packet;
    }

    void cBeam::SetSize(const cVector2f& avSize)
	{
		mvSize = avSize;
		mBoundingVolume.SetSize(cVector3f(mvSize.x, mvSize.y, mvSize.x));

		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	void cBeam::SetTileHeight(bool abX)
	{
		if(mbTileHeight == abX) return;

		mbTileHeight = abX;

		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	void cBeam::SetMultiplyAlphaWithColor(bool abX)
	{
		if(mbMultiplyAlphaWithColor == abX) return;

		mbMultiplyAlphaWithColor = abX;
	}

	//-----------------------------------------------------------------------

	void cBeam::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;

		mColor = aColor;
	}

	void cBeam::SetMaterial(cMaterial * apMaterial)
	{
		mpMaterial = apMaterial;
	}

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

	void cBeam::UpdateGraphicsForFrame(float afFrameTime)
	{
		if(	mlStartTransformCount == GetTransformUpdateCount() &&
			mlEndTransformCount == GetTransformUpdateCount())
		{
			return;
		}
        m_activeCopy = (m_activeCopy + 1) % 2;

		////////////////////////////////
		//Get Axis
		mvAxis = mpEnd->GetWorldPosition() - GetWorldPosition();

		mvMidPosition = GetWorldPosition() + mvAxis*0.5f;
		float fDist = mvAxis.Length();

		mvAxis.Normalize();

		//Update vertex buffer
		cVector2f vBeamSize = cVector2f(mvSize.x, fDist);
        const std::array<float3, BeamNumberVerts> vCoords = { float3((vBeamSize.x / 2), -(vBeamSize.y / 2), 0.0f),
                                                                   float3(-(vBeamSize.x / 2), -(vBeamSize.y / 2), 0.0f),
                                                                   float3(-(vBeamSize.x / 2), (vBeamSize.y / 2), 0.0f),
                                                                   float3((vBeamSize.x / 2), (vBeamSize.y / 2), 0.0f) };

	    const std::array<float2, BeamNumberVerts> vTexCoords = mbTileHeight ? std::array<float2, BeamNumberVerts> {
			float2(1.0f,1.0f),	//Bottom left
			float2(0.0f,1.0f),	//Bottom right
			float2(0.0f,-fDist/mvSize.y),	//Top left
			float2(1.0f,-fDist/mvSize.y)	//Top right
	    } : std::array<float2, BeamNumberVerts> {
			float2(1.0f,1.0f),	//Bottom left
			float2(0.0f,1.0f),	//Bottom right
			float2(0.0f,0.0f),	//Top left
			 float2(1.0f,0.0f)	//Top right
	    };

        {
            auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
            auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);

            BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle, positionStream->stride() * ((4 * m_activeCopy) + m_geometry->vertexOffset()), positionStream->stride() * 4};
            BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle, textureStream->stride() * ((4 * m_activeCopy) + m_geometry->vertexOffset()), textureStream->stride() * 4};
            BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle, colorStream->stride() * ((4 * m_activeCopy) + m_geometry->vertexOffset()), colorStream->stride() * 4};
            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&textureUpdateDesc);
            beginUpdateResource(&colorUpdateDesc);
            GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
            GraphicsBuffer gpuTextureBuffer(textureUpdateDesc);
            GraphicsBuffer gpuColorBuffer(colorUpdateDesc);

            auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
            auto textureView = gpuTextureBuffer.CreateStructuredView<float2>();
            auto colorView = gpuTextureBuffer.CreateStructuredView<float4>();
            for (int i = 0; i < BeamNumberVerts; ++i) {
                positionView.Write(i, vCoords[i]);
                textureView.Write(i, vTexCoords[i]);
            }
            cColor endColor = mpEnd->mColor;
            cColor startColor = mColor;
            if (mbMultiplyAlphaWithColor) {
                colorView.Write(
                    0, float4(startColor.r * startColor.a, startColor.g * startColor.a, startColor.b * startColor.a, startColor.a));
                colorView.Write(
                    1, float4(startColor.r * startColor.a, startColor.g * startColor.a, startColor.b * startColor.a, startColor.a));
                colorView.Write(2, float4(endColor.r * endColor.a, endColor.g * endColor.a, endColor.b * endColor.a, endColor.a));
                colorView.Write(3, float4(endColor.r * endColor.a, endColor.g * endColor.a, endColor.b * endColor.a, endColor.a));
            } else {
                colorView.Write(0, float4(startColor.r, startColor.g, startColor.b, startColor.a));
                colorView.Write(1, float4(startColor.r, startColor.g, startColor.b, startColor.a));
                colorView.Write(2, float4(endColor.r, endColor.g, endColor.b, endColor.a));
                colorView.Write(3, float4(endColor.r, endColor.g, endColor.b, endColor.a));
            }

            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&textureUpdateDesc);
            endUpdateResource(&colorUpdateDesc);
        }
	}

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

	int cBeam::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

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

	bool cBeam::IsVisible()
	{
		if(mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0) return false;

		return mbIsVisible;
	}


	void cBeamEnd_UpdateCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		cBeamEnd *pEnd = static_cast<cBeamEnd*>(apEntity);

		pEnd->mpBeam->SetTransformUpdated(true);
	}

	void cBeamEnd::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;
		mColor = aColor;
	}

}
