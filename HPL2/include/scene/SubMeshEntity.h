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
#include <folly/small_vector.h>
#include <vector>
#include <map>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/ForgeHandles.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/Entity3D.h"
#include "graphics/Renderable.h"
#include "math/MeshTypes.h"

namespace hpl {

	class cMaterialManager;
	class cMeshManager;
	class cMesh;
	class cSubMesh;
	class cMeshEntity;
	class cAnimationState;
	class cNodeState;
	class cBone;
	class cNode3D;
	class iPhysicsBody;
	class cMaterial;
	class cBoneState;

	class cSubMeshEntity final: public iRenderable
	{
		HPL_RTTI_IMPL_CLASS(iRenderable, cSubMeshEntity, "{285bbdb4-de5b-4960-bf44-ae543432ff40}")
		friend class cMeshEntity;
	public:
        static constexpr uint32_t MaxVertexBindings = 15;

        cSubMeshEntity(const tString &asName,cMeshEntity *apMeshEntity, cSubMesh * apSubMesh,cMaterialManager* apMaterialManager);
		~cSubMeshEntity();

		virtual cMaterial *GetMaterial() override;

		virtual void UpdateGraphicsForFrame(float afFrameTime) override;

		virtual iVertexBuffer* GetVertexBuffer() override;
        virtual DrawPacket ResolveDrawPacket(const ForgeRenderer::Frame& frame,std::span<eVertexBufferElement> elements) override;

		virtual cBoundingVolume* GetBoundingVolume() override;

		virtual cMatrixf* GetModelMatrix(cFrustum *apFrustum) override;

		virtual int GetMatrixUpdateCount() override;

		virtual eRenderableType GetRenderType() override { return eRenderableType_SubMesh;}

		cSubMesh* GetSubMesh() const { return mpSubMesh;}

		void SetLocalNode(cNode3D *apNode);
		cNode3D* GetLocalNode();

		void* GetUserData(){ return mpUserData;}
		void SetUserData(void *apData){ mpUserData = apData;}

		//Entity implementation
		virtual tString GetEntityType() override { return "SubMesh";}

		virtual void UpdateLogic(float afTimeStep) override;

		void SetUpdateBody(bool abX);
		bool GetUpdateBody();

		void SetCustomMaterial(cMaterial *apMaterial, bool abDestroyOldCustom=true);
		cMaterial* GetCustomMaterial(){ return mpMaterial;}

	private:
		virtual void OnTransformUpdated() override;

        enum StreamBufferType {
            StaticBuffer,
            DynamicBuffer
        };

        struct StreamBufferInfo {
        public:
            StreamBufferType m_type = StreamBufferType::StaticBuffer;
            SharedBuffer m_buffer;
            ShaderSemantic m_semantic = ShaderSemantic::SEMANTIC_UNDEFINED;
            uint64_t m_stride = 0;
            uint32_t m_numberElements = 0;
            StreamBufferInfo() {}
            StreamBufferInfo(const StreamBufferInfo& info) = delete;
            StreamBufferInfo(StreamBufferInfo&& info):
                m_type(info.m_type),
                m_buffer(std::move(info.m_buffer)),
                m_semantic(info.m_semantic),
                m_stride(info.m_stride),
                m_numberElements(info.m_numberElements) {
            }
            void operator=(const StreamBufferInfo& info) = delete;
            void operator=(StreamBufferInfo&& info) {
                m_type = info.m_type;
                m_buffer = std::move(info.m_buffer);
                m_semantic = info.m_semantic;
                m_stride = info.m_stride;
                m_numberElements = info.m_numberElements;
            }

        };
        uint8_t m_activeCopy = 0;
        uint32_t m_numberIndecies = 0;
        SharedBuffer m_indexBuffer;
        folly::small_vector<StreamBufferInfo, MaxVertexBindings> m_vertexStreams;

		cSubMesh *mpSubMesh= nullptr;
		cMeshEntity *mpMeshEntity= nullptr;

		cMaterial *mpMaterial= nullptr;

		cNode3D *mpLocalNode = nullptr;

		cMaterialManager* mpMaterialManager= nullptr;

		iVertexBuffer* mpDynVtxBuffer = nullptr;
		tTriangleDataVec mvDynTriangles;

		bool mbUpdateBody = false;
		bool mbGraphicsUpdated = false;
        bool m_skinnedMesh = false;

		//This is used to see if null should be returned.
		// 0 = no check made, test if matrix is identity
		// -1 = Matrix was not identity
		// 1 = matrix was identiy
		char mlStaticNullMatrixCount = 0;
		void *mpUserData;
	};

	typedef std::vector<cSubMeshEntity*> tSubMeshEntityVec;
	typedef std::vector<cSubMeshEntity*>::iterator tSubMeshEntityVecIt;

	typedef std::multimap<tString,cSubMeshEntity*> tSubMeshEntityMap;
	typedef tSubMeshEntityMap::iterator tSubMeshEntityMapIt;
};
