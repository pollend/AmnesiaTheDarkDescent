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

#include "bgfx/bgfx.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "graphics/RenderViewport.h"
#include "math/Frustum.h"
#include "math/MathTypes.h"
#include "scene/SceneTypes.h"

#include "graphics/RenderFunctions.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace hpl {
	

	//---------------------------------------------

	class cGraphics;
	class cResources;
	class cEngine;
	class iLowLevelResources;
	class cMeshCreator;
	class cGpuShaderManager;
	class iRenderable;
	class cWorld;
	class cRenderSettings;
	class cRenderList;
	class iLight;
	class iOcclusionQuery;
	class cBoundingVolume;
	class iRenderableContainer;
	class iRenderableContainerNode;
	class cVisibleRCNodeTracker;
	class RenderCallbackMessage;

	namespace rendering::detail {
		eShadowMapResolution GetShadowMapResolution(eShadowMapResolution aWanted, eShadowMapResolution aMax);
		/**
		* @brief Checks if the given object is occluded by the given planes.
		* @param apObject The object to check.
		* @param alNeededFlags The flags that the object must have to be considered.
		* @param occlusionPlanes The planes to check against.
		*/
		bool IsObjectVisible(iRenderable *apObject, tRenderableFlag alNeededFlags, std::span<cPlanef> occlusionPlanes);
	}

	class cNodeOcclusionPair
	{
	public:
		iRenderableContainerNode *mpNode;
		iOcclusionQuery *mpQuery;
		bool mbObjectsRendered;
	};

	typedef std::list<cNodeOcclusionPair> tNodeOcclusionPairList;
	typedef tNodeOcclusionPairList::iterator tNodeOcclusionPairListIt;

	//---------------------------------------------

	typedef std::multimap<void*, cOcclusionQueryObject*> tOcclusionQueryObjectMap;
	typedef tOcclusionQueryObjectMap::iterator tOcclusionQueryObjectMapIt;

	//---------------------------------------------

	class cLightOcclusionPair
	{
	public:
		cLightOcclusionPair() : mlSampleResults(0) {}

		iLight *mpLight;
		iOcclusionQuery *mpQuery;
		bgfx::OcclusionQueryHandle m_occlusionQuery;
		int32_t mlSampleResults;
	};

	//---------------------------------------------

	class cFogAreaRenderData
	{
	public:
		cFogArea *mpFogArea;
		bool mbInsideNearFrustum;
		cVector3f mvBoxSpaceFrustumOrigin;
		cMatrixf m_mtxInvBoxSpace;
	};


	class iRenderer;
	class cRenderSettings
	{
	public:
		cRenderSettings(bool abIsReflection = false);
		~cRenderSettings();

		////////////////////////////
		//Helper methods
		void ResetVariables();

		void SetupReflectionSettings();

		void AddOcclusionPlane(const cPlanef &aPlane);
		void ResetOcclusionPlanes();

		void AssignOcclusionObject(iRenderer *apRenderer, void *apSource, int alCustomIndex, iVertexBuffer *apVtxBuffer, cMatrixf *apMatrix, bool abDepthTest);
		int RetrieveOcclusionObjectSamples(iRenderer *apRenderer, void *apSource, int alCustomIndex);
		void ClearOcclusionObjects(iRenderer *apRenderer);
		void WaitAndRetrieveAllOcclusionQueries(iRenderer *apRenderer);

		////////////////////////////
		//Data
		cRenderList *mpRenderList;

		cVisibleRCNodeTracker *mpVisibleNodeTracker;

		cRenderSettings *mpReflectionSettings;

		std::vector<iRenderableContainerNode*> m_testNodes;

		int mlCurrentOcclusionObject;
		std::vector<cOcclusionQueryObject*> mvOcclusionObjectPool;
		tOcclusionQueryObjectMap m_setOcclusionObjects;

		std::vector<cFogAreaRenderData> mvFogRenderData;

        ////////////////////////////
		//General settings
		bool mbLog;

		cColor mClearColor;

		////////////////////////////
		//Render settings
		int mlMinimumObjectsBeforeOcclusionTesting;
		int mlSampleVisiblilityLimit;
		bool mbIsReflection;
		bool mbClipReflectionScreenRect;

		bool mbUseOcclusionCulling;

		bool mbUseEdgeSmooth;

		tPlanefVec mvOcclusionPlanes;

		bool mbUseCallbacks;

		eShadowMapResolution mMaxShadowMapResolution;

		bool mbUseScissorRect;
		cVector2l mvScissorRectPos;
		cVector2l mvScissorRectSize;

		bool mbRenderWorldReflection;

		////////////////////////////
		//Shadow settings
		bool mbRenderShadows;
		float mfShadowMapBias;
		float mfShadowMapSlopeScaleBias;

		////////////////////////////
		//Light settings
		bool mbSSAOActive;

		////////////////////////////
		//Output
		int mlNumberOfLightsRendered;
		int mlNumberOfOcclusionQueries;


	};

	//---------------------------------------------

	class cRendererNodeSortFunc
	{
	public:
		bool operator()(const iRenderableContainerNode* apNodeA, const iRenderableContainerNode* apNodeB) const;
	};


	typedef std::multiset<iRenderableContainerNode*, cRendererNodeSortFunc> tRendererSortedNodeSet;
	typedef tRendererSortedNodeSet::iterator tRendererSortedNodeSetIt;

	//---------------------------------------------

	class cShadowMapLightCache
	{
	public:
		cShadowMapLightCache() : mpLight(NULL), mlTransformCount(-1), mfRadius(0),mfFOV(0), mfAspect(0) {}

		void SetFromLight(iLight* apLight);

		iLight *mpLight;
		int mlTransformCount;
		float mfRadius;
		float mfFOV;
		float mfAspect;
	};

	//---------------------------------------------

	class cShadowMapData
	{
	public:
		// iTexture *mpTempDiffTexture;
		// iTexture *mpTexture;
		// iFrameBuffer *mpBuffer;
		
		int mlFrameCount;
		RenderTarget m_target;
		cShadowMapLightCache mCache;
	};

	//---------------------------------------------


	class iRenderer : public iRenderFunctions
	{
	friend class cRendererCallbackFunctions;
	friend class cRenderSettings;

	public:

		iRenderer(const tString& asName, cGraphics *apGraphics,cResources* apResources, int alNumOfProgramComboModes);
		virtual ~iRenderer();

		// plan to just use the single draw call need to call BeginRendering to setup state
		// ensure the contents is copied to the RenderViewport
		virtual void Draw(GraphicsContext& context, float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, RenderViewport& apRenderTarget,
					bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList) {} ;

		[[deprecated("Use Draw instead")]]
		void Render(float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, const RenderViewport& apRenderTarget,
					bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList);

		void Update(float afTimeStep);

		inline static int GetRenderFrameCount()  { return mlRenderFrameCount;}
		inline static void IncRenderFrameCount() { ++mlRenderFrameCount;}

		float GetTimeCount(){ return mfTimeCount;}

		virtual bool LoadData()=0;
		virtual void DestroyData()=0;

		virtual std::shared_ptr<Image> GetDepthStencilImage() { return std::shared_ptr<Image>(nullptr);}
		virtual std::shared_ptr<Image> GetOutputImage() { return std::shared_ptr<Image>(nullptr);}

		cWorld *GetCurrentWorld(){ return mpCurrentWorld;}
		cFrustum *GetCurrentFrustum(){ return mpCurrentFrustum;}
		cRenderList *GetCurrentRenderList(){ return mpCurrentRenderList;}

		iVertexBuffer* GetShapeBoxVertexBuffer(){ return mpShapeBox; }

		void AssignOcclusionObject(void *apSource, int alCustomIndex, iVertexBuffer *apVtxBuffer, cMatrixf *apMatrix, bool abDepthTest);
		int RetrieveOcclusionObjectSamples(void *apSource, int alCustomIndex);
		/**
		* Retrieves number of samples for all active occlusion queries. It does not release the queries, but the values are gotten and stored in query.
		*/
		void WaitAndRetrieveAllOcclusionQueries();


		//Temp variables used by material.
		float GetTempAlpha(){ return mfTempAlpha; }

		//Static settings. Must be set before renderer data load.
		static void SetShadowMapQuality(eShadowMapQuality aQuality) { mShadowMapQuality = aQuality;}
		static eShadowMapQuality GetShadowMapQuality(){ return mShadowMapQuality;}

		static void SetShadowMapResolution(eShadowMapResolution aResolution) { mShadowMapResolution = aResolution;}
		static eShadowMapResolution GetShadowMapResolution(){ return mShadowMapResolution;}

		static void SetParallaxQuality(eParallaxQuality aQuality) { mParallaxQuality = aQuality;}
		static eParallaxQuality GetParallaxQuality(){ return mParallaxQuality;}

		static void SetParallaxEnabled(bool abX) { mbParallaxEnabled = abX;}
		static bool GetParallaxEnabled(){ return mbParallaxEnabled;}

		static void SetReflectionSizeDiv(int alX) { mlReflectionSizeDiv = alX;}
		static int GetReflectionSizeDiv(){ return mlReflectionSizeDiv;}

		static void SetRefractionEnabled(bool abX) { mbRefractionEnabled = abX;}
		static bool GetRefractionEnabled(){ return mbRefractionEnabled;}

		//Debug
		tRenderableVec *GetShadowCasterVec(){ return &mvShadowCasters;}

	protected:
		// a utility to collect renderable objects from the current render list
		void RenderableHelper(eRenderListType type, eMaterialRenderMode mode, std::function<void(iRenderable* obj, GraphicsContext::LayoutStream&, GraphicsContext::ShaderProgram&)> handler);

		void BeginRendering(float afFrameTime,cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, const RenderViewport& apRenderTarget,
							bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList, bool abAtStartOfRendering=true);

		cShadowMapData* GetShadowMapData(eShadowMapResolution aResolution, iLight *apLight);
		bool ShadowMapNeedsUpdate(iLight *apLight, cShadowMapData *apShadowData);
		void DestroyShadowMaps();


        void RenderZObject(GraphicsContext& context, iRenderable *apObject, cFrustum *apCustomFrustum);

		/**
		 * Brute force adding of visible objects. Nothing is rendered.
		 */
		void CheckForVisibleAndAddToList(iRenderableContainer *apContainer, tRenderableFlag alNeededFlags);

		void CheckNodesAndAddToListIterative(iRenderableContainerNode *apNode, tRenderableFlag alNeededFlags);


		void OcclusionQueryBoundingBoxTest(bgfx::ViewId view, 
			GraphicsContext& context, 
			bgfx::OcclusionQueryHandle handle, 
			const cFrustum& frustum,
			const cMatrixf& transform, 
			RenderTarget& rt,Cull cull = Cull::CounterClockwise);
		/**
		 * Uses a Coherent occlusion culling to get visible objects. No early Z needed after calling this
		 */
		void RenderZPassWithVisibilityCheck(GraphicsContext& context, cVisibleRCNodeTracker *apVisibleNodeTracker, tRenderableFlag alNeededFlags, tObjectVariabilityFlag variabilityFlag,
			RenderTarget& rt, std::function<bool(bgfx::ViewId view, iRenderable* object)> renderHandler);

		void PushUpVisibility(iRenderableContainerNode *apNode);
		void RenderNodeBoundingBox(iRenderableContainerNode *apNode, iOcclusionQuery *apQuery);
		
		void PushNodeChildrenToStack(tRendererSortedNodeSet& a_setNodeStack, iRenderableContainerNode *apNode, int alNeededFlags);
		void AddAndRenderNodeOcclusionQuery(tNodeOcclusionPairList *apList, iRenderableContainerNode *apNode, bool abObjectsRendered);

		bool CheckShadowCasterContributesToView(iRenderable *apObject);
		void GetShadowCastersIterative(iRenderableContainerNode *apNode, eCollision aPrevCollision);
		void GetShadowCasters(iRenderableContainer *apContainer, tRenderableVec& avObjectVec, cFrustum *apLightFrustum);
		bool SetupShadowMapRendering(iLight *apLight);

		bool RenderShadowCasterCHC(iRenderable *apObject);
		void RenderShadowCaster(iRenderable *apObject, cFrustum *apLightFrustum);
	
		/**
		 * Only depth is needed for framebuffer. All objects needs to be added to renderlist!
		 */
		void AssignAndRenderOcclusionQueryObjects(bgfx::ViewId view, GraphicsContext& context, bool abSetFrameBuffer, bool abUsePosAndSize);

		/**
		 * Checks if the renderable object is 1) submeshentity 2) is onesided plane 3)is away from camera. If all are true, FALSE is returned.
		 */
		bool CheckRenderablePlaneIsVisible(iRenderable *apObject, cFrustum *apFrustum);
		/**
		 * afHalfFovTan == 0 means that the function calculates tan.
		 */
		cRect2l GetClipRectFromObject(iRenderable *apObject, float afPaddingPercent, cFrustum *apFrustum, const cVector2l &avScreenSize, float afHalfFovTan);


		/**
		* Checks the IsVisible and also clip planes (in setttings), if visible in reflection and other stuff . No frustum check!
		*/
		bool CheckObjectIsVisible(iRenderable *apObject, tRenderableFlag alNeededFlags);

		/**
		 * Checks custom clip planes (in setttings) and more to determine if a node is viisible. No frustum check!
		 */
		[[deprecated("use free method rendering::detail::IsObjectVisible")]]
		bool CheckNodeIsVisible(iRenderableContainerNode *apNode);

		bool CheckFogAreaInsideNearPlane(cMatrixf &a_mtxInvBoxModelMatrix);
		void SetupFogRenderDataArray(bool abSort);
		bool CheckFogAreaRayIntersection(	cMatrixf &a_mtxInvBoxModelMatrix, const cVector3f &avBoxSpaceRayStart, const cVector3f& avRayDir,
											float &afEntryDist, float &afExitDist);
		float GetFogAreaVisibilityForObject(cFogAreaRenderData *apFogData, iRenderable *apObject);

		void SetOcclusionPlanesActive(bool abX);

		iVertexBuffer* LoadVertexBufferFromMesh(const tString& asMeshName, tVertexElementFlag alVtxToCopy);
	
		void RunCallback(eRendererMessage aMessage, cRendererCallbackFunctions& handler);

        cResources* mpResources;
		bgfx::ProgramHandle m_nullShader;

		tString msName;

		float mfDefaultAlphaLimit;

		bool mbSetFrameBufferAtBeginRendering;
		bool mbClearFrameBufferAtBeginRendering;
		bool mbSetupOcclusionPlaneForFog;

		bool mbOnlyRenderPrevVisibleOcclusionObjects;
		int mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount;

		float mfCurrentFrameTime;
		cWorld *mpCurrentWorld;
		cRenderSettings *mpCurrentSettings;
		cRenderList *mpCurrentRenderList;

		bool mbSendFrameBufferToPostEffects;
		tRendererCallbackList *mpCallbackList;

		float mfCurrentNearPlaneTop;
		float mfCurrentNearPlaneRight;

		iVertexBuffer *mpShapeBox;

		cMatrixf m_mtxSkyBox;

		cRect2l mTempClipRect;
		float mfScissorLastFov;
		float mfScissorLastTanHalfFov;

		std::vector<iRenderable*> mvShadowCasters;

		static int mlRenderFrameCount;
		float mfTimeCount;

		bool mbOcclusionPlanesActive;
		std::vector<cPlanef> mvCurrentOcclusionPlanes;

		int mlActiveOcclusionQueryNum;
		std::vector<cOcclusionQueryObject*> mvSortedOcclusionObjects;
		std::array<absl::InlinedVector<cShadowMapData, 10>, eShadowMapResolution_LastEnum> m_shadowMapData;

		float mfTempAlpha;

		static eShadowMapQuality mShadowMapQuality;
		static eShadowMapResolution mShadowMapResolution;
		static eParallaxQuality mParallaxQuality;
		static bool mbParallaxEnabled;
		static int mlReflectionSizeDiv;
		static bool mbRefractionEnabled;
	};

	//---------------------------------------------

	class cRendererCallbackFunctions
	{
	public:
		cRendererCallbackFunctions(GraphicsContext& context,iRenderer *apRenderer) : m_context(context), mpRenderer(apRenderer) {}

		cRenderSettings* GetSettings(){ return mpRenderer->mpCurrentSettings;}
		cFrustum* GetFrustum(){ return mpRenderer->mpCurrentFrustum;}

		inline void SetFlatProjection(const cVector2f &avSize=1,float afMin=-100,float afMax=100) { mpRenderer->SetFlatProjection(avSize, afMin,afMax); }
		inline void SetFlatProjectionMinMax(const cVector3f &avMin,const cVector3f &avMax) { mpRenderer->SetFlatProjectionMinMax(avMin,avMax);}
		inline void SetNormalFrustumProjection() { mpRenderer->SetNormalFrustumProjection(); }

		inline void DrawQuad(	const cVector3f& aPos, const cVector2f& avSize, const cVector2f& avMinUV=0, const cVector2f& avMaxUV=1,
								bool abInvertY=false, const cColor& aColor=cColor(1,1) )
							{ mpRenderer->DrawQuad(aPos,avSize,avMinUV,avMaxUV,abInvertY,aColor); }

		inline bool SetDepthTest(bool abX){ return mpRenderer->SetDepthTest(abX); }
		inline bool SetDepthWrite(bool abX){ return mpRenderer->SetDepthWrite(abX); }
		inline bool SetDepthTestFunc(eDepthTestFunc aFunc){ return mpRenderer->SetDepthTestFunc(aFunc); }
		inline bool SetCullActive(bool abX){ return mpRenderer->SetCullActive(abX); }
		inline bool SetCullMode(eCullMode aMode){ return mpRenderer->SetCullMode(aMode); }
		inline bool SetStencilActive(bool abX){ return mpRenderer->SetStencilActive(abX); }
		inline bool SetScissorActive(bool abX){ return mpRenderer->SetScissorActive(abX); }
		inline bool SetScissorRect(const cVector2l& avPos, const cVector2l& avSize, bool abAutoEnabling){return mpRenderer->SetScissorRect(avPos, avSize, abAutoEnabling);}
		inline bool SetScissorRect(const cRect2l& aClipRect, bool abAutoEnabling){return mpRenderer->SetScissorRect(aClipRect,abAutoEnabling);}
		inline bool SetChannelMode(eMaterialChannelMode aMode){ return mpRenderer->SetChannelMode(aMode); }
		inline bool SetAlphaMode(eMaterialAlphaMode aMode){ return mpRenderer->SetAlphaMode(aMode); }
		inline bool SetBlendMode(eMaterialBlendMode aMode){ return mpRenderer->SetBlendMode(aMode); }
		inline bool SetProgram(iGpuProgram *apProgram){ return mpRenderer->SetProgram(apProgram); }
		inline void SetTexture(int alUnit, iTexture *apTexture){ mpRenderer->SetTexture(alUnit, apTexture); }
		inline void SetTextureRange(iTexture *apTexture, int alFirstUnit, int alLastUnit = kMaxTextureUnits-1){ mpRenderer->SetTextureRange(apTexture,alFirstUnit,alLastUnit); }
		inline void SetVertexBuffer(iVertexBuffer *apVtxBuffer){ mpRenderer->SetVertexBuffer(apVtxBuffer); }
		inline void SetMatrix(cMatrixf *apMatrix){ mpRenderer->SetMatrix(apMatrix); }
		inline void SetModelViewMatrix(const cMatrixf& a_mtxModelView){ mpRenderer->SetModelViewMatrix(a_mtxModelView); }

		inline void DrawCurrent(eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum){ mpRenderer->DrawCurrent(aDrawType); }

		void DrawWireFrame(iVertexBuffer *apVtxBuffer, const cColor &aColor){ mpRenderer->DrawWireFrame(apVtxBuffer, aColor);}

		inline GraphicsContext& GetGraphicsContext(){ return m_context; }

		iLowLevelGraphics *GetLowLevelGfx(){ return mpRenderer->mpLowLevelGraphics;}

	private:
		iRenderer *mpRenderer;
		GraphicsContext& m_context;
	};

	//---------------------------------------------

};
