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

#include "bgfx/bgfx.h"
#include "engine/Interface.h"
#include "impl/VertexBufferBGFX.h"
#include "math/MathTypes.h"
#ifdef WIN32
//#pragma comment(lib, "OpenGL32.lib")
//#pragma comment(lib, "GLu32.lib")
//#pragma comment(lib, "GLaux.lib")
//#pragma comment(lib, "Cg.lib")
//#pragma comment(lib, "CgGL.lib")
//#pragma comment(lib, "SDL_ttf.lib")
//#pragma comment(lib, "TaskKeyHook.lib")
#endif

#include <assert.h>
#include <stdlib.h>

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/Mutex.h"

#include "impl/LowLevelGraphicsSDL.h"
#include "impl/SDLFontData.h"

#include "graphics/Bitmap.h"
#include <windowing/NativeWindow.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif

#include "SDL2/SDL_syswm.h"

#ifdef WIN32
#include "impl/TaskKeyHook.h"
#endif

#ifndef WIN32
	#if defined __ppc__ || defined(__LP64__)
		#define CALLBACK
	#else
		#define CALLBACK __attribute__ ((__stdcall__))
	#endif
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::cLowLevelGraphicsSDL()
	{
		mlBatchArraySize = 20000;
		mlVertexCount = 0;
		mlIndexCount =0;
		mlMultisampling =0;

		mbDoubleSidedStencilIsSet = false;

#if defined(WIN32) && !SDL_VERSION_ATLEAST(2,0,0)
		mhKeyTrapper = NULL;
#endif

		mbInitHasBeenRun = false;


		//TTF_Init();
	}

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::~cLowLevelGraphicsSDL()
	{


	}

	bool cLowLevelGraphicsSDL::Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen,
		int alMultisampling, eGpuProgramFormat aGpuProgramFormat,const tString& asWindowCaption,
		const cVector2l &avWindowPos)
	{
        mlDisplay = alDisplay;
		mlBpp = alBpp;
		mbFullscreen = abFullscreen;

		mlMultisampling = alMultisampling;

		mGpuProgramFormat = aGpuProgramFormat;
		if(mGpuProgramFormat == eGpuProgramFormat_LastEnum) mGpuProgramFormat = eGpuProgramFormat_GLSL;

            SetWindowGrab(true);
     
		// Log(" Init Glew...");
		// if(glewInit() == GLEW_OK)
		// {
		// 	Log("OK\n");
		// }
		// else
		// {
		// 	Error(" Couldn't init glew!\n");
		// }

		///Setup up windows specifc context:
		//Check Multisample properties
		CheckMultisampleCaps();

		//Turn off cursor as default
		ShowCursor(false);

        // SDL_SetWindowBrightness(mpScreen, mfGammaCorrection);

		mbInitHasBeenRun = true;


		mbDepthWrite = true;

		mbCullActive = true;
		mCullMode = eCullMode_CounterClockwise;

		mbDepthTestActive = true;
		mDepthTestFunc = eDepthTestFunc_LessOrEqual;

		mbAlphaTestActive = false;
		mAlphaTestFunc = eAlphaTestFunc_GreaterOrEqual;
		mfAlphaTestFuncRef = 0.6f;

		mbScissorActive = false;
		mvScissorPos =0;
		// mvScissorSize = mvScreenSize;

		mbBlendActive = false;

		// mvFrameBufferPos =0;
		// mvFrameBufferSize = mvScreenSize;
		// mvFrameBufferTotalSize = mvScreenSize;

		return true;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::CheckMultisampleCaps()
	{

	}
	
	//-----------------------------------------------------------------------

	int cLowLevelGraphicsSDL::GetCaps(eGraphicCaps aType)
	{

		return 0;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ShowCursor(bool abX)
	{
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			if(abX) {
				window->ShowHardwareCursor();
			} else {
				window->HideHardwareCursor();
			}
		}
	}

	//-----------------------------------------------------------------------

    void cLowLevelGraphicsSDL::SetWindowGrab(bool abX)
    {
        if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			if(abX) {
				window->WindowGrabCursor();
			} else {
				window->WindowReleaseCursor();
			}
		}
    }

	void cLowLevelGraphicsSDL::SetRelativeMouse(bool abX)
	{
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			if(abX) {
				window->ConstrainCursor();
			} else {
				window->UnconstrainCursor();
			}
		}
	}

    void cLowLevelGraphicsSDL::SetWindowCaption(const tString &asName)
    {
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			window->SetWindowTitle(asName);
		}
    }

    bool cLowLevelGraphicsSDL::GetWindowMouseFocus()
    {
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			return any(window->GetWindowStatus() & window::WindowStatus::WindowStatusInputMouseFocus);
		}
		return false;
    }

    bool cLowLevelGraphicsSDL::GetWindowInputFocus()
    {
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			return any(window->GetWindowStatus() & window::WindowStatus::WindowStatusInputFocus);
		}
		return false;
    }

    bool cLowLevelGraphicsSDL::GetWindowIsVisible()
    {
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			return any(window->GetWindowStatus() & window::WindowStatus::WindowStatusVisible);
		}
		return false;
    }

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVsyncActive(bool abX, bool abAdaptive)
	{
//         ;
// #if SDL_VERSION_ATLEAST(2, 0, 0)
//         SDL_GL_SetSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
// #elif defined(WIN32)
// 		if(WGLEW_EXT_swap_control)
// 		{
// 			wglSwapIntervalEXT(abX ? (abAdaptive ? -1 : 1) : 0);
// 		}
// #elif defined(__linux__) || defined(__FreeBSD__)
// 		if (GLX_SGI_swap_control)
// 		{
// 			GLXSWAPINTERVALPROC glXSwapInterval = (GLXSWAPINTERVALPROC)glXGetProcAddress((GLubyte*)"glXSwapIntervalSGI");
// 			glXSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
// 		}
// 		else if (GLX_MESA_swap_control)
// 		{
// 			GLXSWAPINTERVALPROC glXSwapInterval = (GLXSWAPINTERVALPROC)glXGetProcAddress((GLubyte*)"glXSwapIntervalMESA");
// 			glXSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
// 		}
// #elif defined(__APPLE__)
// 		CGLContextObj ctx = CGLGetCurrentContext();
// 		GLint swap = abX ? 1 : 0;
// 		CGLSetParameter(ctx, kCGLCPSwapInterval, &swap);
// #endif
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMultisamplingActive(bool abX)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetGammaCorrection(float afX)
	{
		mfGammaCorrection = afX;
        // SDL_SetWindowBrightness(hpl::entry_sdl::getWindow(), mfGammaCorrection);
	}

	float cLowLevelGraphicsSDL::GetGammaCorrection()
	{
		return mfGammaCorrection;
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetScreenSizeFloat()
	{
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			auto size = window->GetWindowSize();
			return cVector2f(static_cast<float>(size.x), static_cast<float>(size.y));
		}
		return cVector2f(0,0);
	}

	const cVector2l cLowLevelGraphicsSDL::GetScreenSizeInt()
	{
		if(auto* window = Interface<window::NativeWindowWrapper>::Get()) {
			return window->GetWindowSize();
		}
		return cVector2l(0,0);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// DATA CREATION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iFontData* cLowLevelGraphicsSDL::CreateFontData(const tString &asName)
	{
		;

		return hplNew( cSDLFontData, (asName, this) );
	}

	// //-----------------------------------------------------------------------
	iGpuProgram* cLowLevelGraphicsSDL::CreateGpuProgram(const tString& asName)
	{
		BX_ASSERT(false, "deprecated CreateGpuProgram");
		return nullptr;
	}

	iGpuShader* cLowLevelGraphicsSDL::CreateGpuShader(const tString& asName, eGpuShaderType aType)
	{
		BX_ASSERT(false, "deprecated CreateGpuProgram");
		return nullptr;
	}

	//-----------------------------------------------------------------------

	iTexture* cLowLevelGraphicsSDL::CreateTexture(const tString &asName,eTextureType aType,   eTextureUsage aUsage)
	{
		BX_ASSERT(false, "deprecated CreateTexture");
		return nullptr;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cLowLevelGraphicsSDL::CreateVertexBuffer(eVertexBufferType aType,
															eVertexBufferDrawType aDrawType,
															eVertexBufferUsageType aUsageType,
															int alReserveVtxSize,int alReserveIdxSize)
	{
		return new iVertexBufferBGFX(this,eVertexBufferType_Hardware, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize);
	}

	//-----------------------------------------------------------------------

	iFrameBuffer* cLowLevelGraphicsSDL::CreateFrameBuffer(const tString& asName)
	{
		BX_ASSERT(false, "interface is deprecated");
		return nullptr;
	}

	//-----------------------------------------------------------------------

	iDepthStencilBuffer* cLowLevelGraphicsSDL::CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits)
	{
		BX_ASSERT(false, "interface is deprecated");
		return nullptr;
	}

	void cLowLevelGraphicsSDL::ClearFrameBuffer(tClearFrameBufferFlag aFlags)
	{
		BX_ASSERT(false, "interface is deprecated");
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetClearColor(const cColor& aCol){
		BX_ASSERT(false, "interface is deprecated");
	}
	void cLowLevelGraphicsSDL::SetClearDepth(float afDepth){
		BX_ASSERT(false, "interface is deprecated");

	}
	void cLowLevelGraphicsSDL::SetClearStencil(int alVal){
		BX_ASSERT(false, "interface is deprecated");

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::CopyFrameBufferToTexure(iTexture* apTex, const cVector2l &avPos,
		const cVector2l &avSize, const cVector2l &avTexOffset)
	{
	}

	//-----------------------------------------------------------------------

	cBitmap* cLowLevelGraphicsSDL::CopyFrameBufferToBitmap(	const cVector2l &avScreenPos,const cVector2l &avScreenSize)
	{
		BX_ASSERT(false, "TODO: need to replace this with a helper method no need to couple this");

		return nullptr;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l &avPos, const cVector2l& avSize)
	{
		BX_ASSERT(false, "Deprecated");
	}

	void cLowLevelGraphicsSDL::WaitAndFinishRendering()
	{	
		BX_ASSERT(false, "Deprecated");
	}

	void cLowLevelGraphicsSDL::FlushRendering()
	{
		BX_ASSERT(false, "Deprecated");
	}

	void cLowLevelGraphicsSDL::SwapBuffers()
	{
		BX_ASSERT(false, "Deprecated");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RENDER STATE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColorWriteActive(bool abR,bool abG,bool abB,bool abA)
	{

	}

	void cLowLevelGraphicsSDL::SetDepthWriteActive(bool abX)
	{
	}

	void cLowLevelGraphicsSDL::SetDepthTestActive(bool abX)
	{
	}

	void cLowLevelGraphicsSDL::SetDepthTestFunc(eDepthTestFunc aFunc)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestActive(bool abX)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestFunc(eAlphaTestFunc aFunc,float afRef)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilActive(bool abX)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilWriteMask(unsigned int alMask)
	{
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencil(eStencilFunc aFunc,int alRef, unsigned int aMask,
		eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilTwoSide(eStencilFunc aFrontFunc,eStencilFunc aBackFunc,
		int alRef, unsigned int aMask,
		eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
		eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCullActive(bool abX)
	{
	}

	void cLowLevelGraphicsSDL::SetCullMode(eCullMode aMode)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorActive(bool abX)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize)
	{
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetClipPlane(int alIdx, const cPlanef& aPlane)
	{
	}
	cPlanef cLowLevelGraphicsSDL::GetClipPlane(int alIdx)
	{
		return cPlanef();

	}
	void cLowLevelGraphicsSDL::SetClipPlaneActive(int alIdx, bool abX)
	{
	}


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendActive(bool abX)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor)
	{
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor,
		eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetPolygonOffsetActive(bool abX)
	{

	}


	void cLowLevelGraphicsSDL::SetPolygonOffset(float afBias, float afSlopeScaleBias)
	{
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PushMatrix(eMatrix aMtxType)
	{
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PopMatrix(eMatrix aMtxType)
	{
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA)
	{
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetIdentityMatrix(eMatrix aMtxType)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax)
	{
	}

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax)
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// TEXTURE OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetTexture(unsigned int alUnit,iTexture* apTex)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColor(const cColor &aColor)
	{
	}

	//-----------------------------------------------------------------------



	//////////////////////////////////////////////////////////////////////////
	// DRAWING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawTriangle(tVertexVec& avVtx)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,const cColor& aColor)
	{

	}

	void cLowLevelGraphicsSDL::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,
		const cVector2f &avMinTexCoord,const cVector2f &avMaxTexCoord,
		const cColor& aColor)
	{

	}

	void cLowLevelGraphicsSDL::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,
		const cVector2f &avMinTexCoord0,const cVector2f &avMaxTexCoord0,
		const cVector2f &avMinTexCoord1,const cVector2f &avMaxTexCoord1,
		const cColor& aColor)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuad(const tVertexVec &avVtx)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuadMultiTex(const tVertexVec &avVtx,const tVector3fVec &avExtraUvs)
	{

	}


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuad(const tVertexVec &avVtx, const cColor aCol)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuad(const tVertexVec &avVtx,const float afZ)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawQuad(const tVertexVec &avVtx,const float afZ,const cColor &aCol)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawLine(const cVector3f& avBegin, const cVector3f& avEnd, cColor aCol)
	{

	}

	void cLowLevelGraphicsSDL::DrawLine(const cVector3f& avBegin, const cColor& aBeginCol, const cVector3f& avEnd, const cColor& aEndCol)
	{

	}

	void cLowLevelGraphicsSDL::DrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, cColor aCol)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawSphere(const cVector3f& avPos, float afRadius, cColor aCol)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawSphere(const cVector3f& avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DrawLineQuad(const cRect2f& aRect, float afZ, cColor aCol)
	{
	}

	void cLowLevelGraphicsSDL::DrawLineQuad(const cVector3f &avPos,const cVector2f &avSize, cColor aCol)
	{
	}



	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// VERTEX BATCHING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddVertexToBatch(const cVertex *apVtx)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddVertexToBatch(const cVertex *apVtx, const cVector3f* avTransform)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddVertexToBatch(const cVertex *apVtx, const cMatrixf* aMtx)
	{
		;

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddVertexToBatch_Size2D(const cVertex *apVtx, const cVector3f* avTransform,
		const cColor* apCol,const float& mfW, const float& mfH)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddVertexToBatch_Raw(	const cVector3f& avPos, const cColor &aColor,
		const cVector3f& avTex)
	{
	}


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddIndexToBatch(int alIndex)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::AddTexCoordToBatch(unsigned int alUnit,const cVector3f *apCoord)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBatchTextureUnitActive(unsigned int alUnit,bool abActive)
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ClearBatch()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// IMPLEMENTAION SPECIFICS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetUpBatchArrays()
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVtxBatchStates(tVtxBatchFlag aFlags)
	{
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMatrixMode(eMatrix mType)
	{
	}

}
