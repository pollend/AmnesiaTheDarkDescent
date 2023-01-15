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
#include "impl/VertexBufferBGFX.h"
#include "math/MathTypes.h"
#ifdef WIN32
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLu32.lib")
//#pragma comment(lib, "GLaux.lib")
#pragma comment(lib, "Cg.lib")
#pragma comment(lib, "CgGL.lib")
//#pragma comment(lib, "SDL_ttf.lib")
#pragma comment(lib, "TaskKeyHook.lib")
#endif

#include <assert.h>
#include <stdlib.h>

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/Mutex.h"

#include "impl/LowLevelGraphicsSDL.h"
#include "impl/SDLFontData.h"
#include "impl/SDLTexture.h"
//#include "impl/CGShader.h"
//#include "impl/CGProgram.h"
#include "impl/GLSLShader.h"
#include "impl/GLSLProgram.h"
#include "impl/VertexBufferOGL_Array.h"
#include "impl/VertexBufferOGL_VBO.h"
#include "impl/FrameBufferGL.h"
#include "impl/OcclusionQueryOGL.h"

#include <graphics/EntrySDL.h>
#include "graphics/Bitmap.h"

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

	#ifdef HPL_OGL_THREADSAFE
		iMutex *gpLowlevelGfxMutex=NULL;

		cLowlevelGfxMutex::cLowlevelGfxMutex(){ if(gpLowlevelGfxMutex) gpLowlevelGfxMutex->Lock(); }
		cLowlevelGfxMutex::~cLowlevelGfxMutex(){ if(gpLowlevelGfxMutex) gpLowlevelGfxMutex->Unlock(); }
	#endif

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
        mbGrab = false;

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

		//Exit extra stuff
#ifdef WITH_CG
		ExitCG();
#endif

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// GENERAL SETUP
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void CALLBACK OGLDebugOutputCallback(GLenum alSource, GLenum alType, GLuint alID, GLenum alSeverity, GLsizei alLength, const GLchar* apMessage, GLvoid* apUserParam)
	{
		Log("Source: %d Type: %d Id: %d Severity: %d '%s'\n", alSource, alType, alID, alSeverity, apMessage);
	}

	//-----------------------------------------------------------------------

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

        if (mbGrab) {
            SetWindowGrab(true);
        }


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

		///////////////////////////////
		// Setup variables
		mColorWrite.r = true; mColorWrite.g = true;
		mColorWrite.b = true; mColorWrite.a = true;
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
		;

		switch(aType)
		{
		case eGraphicCaps_TextureTargetRectangle:	return 1;//GLEW_ARB_texture_rectangle?1:0;

		case eGraphicCaps_VertexBufferObject:		return GLEW_ARB_vertex_buffer_object?1:0;
		case eGraphicCaps_TwoSideStencil:
			{
				if(GLEW_EXT_stencil_two_side) return 1;
				else if(GLEW_ATI_separate_stencil) return 1;
				else return 0;
			}

		case eGraphicCaps_MaxTextureImageUnits:
			{
				int lUnits;
				glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB,(GLint *)&lUnits);
				return lUnits;
			}

		case eGraphicCaps_MaxTextureCoordUnits:
			{
				int lUnits;
				glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB,(GLint *)&lUnits);
				return lUnits;
			}
		case eGraphicCaps_MaxUserClipPlanes:
			{
				int lClipPlanes;
				glGetIntegerv( GL_MAX_CLIP_PLANES,(GLint *)&lClipPlanes);
				return lClipPlanes;
			}

		case eGraphicCaps_AnisotropicFiltering:		return GLEW_EXT_texture_filter_anisotropic ? 1 : 0;

		case eGraphicCaps_MaxAnisotropicFiltering:
			{
				if(!GLEW_EXT_texture_filter_anisotropic) return 0;

				float fMax;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&fMax);
				return (int)fMax;
			}

		case eGraphicCaps_Multisampling: return GLEW_ARB_multisample ? 1: 0;

		case eGraphicCaps_TextureCompression:		return GLEW_ARB_texture_compression  ? 1 : 0;
		case eGraphicCaps_TextureCompression_DXTC:	return GLEW_EXT_texture_compression_s3tc ? 1 : 0;

		case eGraphicCaps_AutoGenerateMipMaps:		return GLEW_SGIS_generate_mipmap ? 1 : 0;

		case eGraphicCaps_RenderToTexture:			return GLEW_EXT_framebuffer_object ? 1: 0;

		case eGraphicCaps_MaxDrawBuffers:
			{
				GLint lMaxbuffers;
				glGetIntegerv(GL_MAX_DRAW_BUFFERS, &lMaxbuffers);
				return lMaxbuffers;
			}
		case eGraphicCaps_PackedDepthStencil:	return GLEW_EXT_packed_depth_stencil ? 1: 0;
		case eGraphicCaps_TextureFloat:			return GLEW_ARB_texture_float ? 1: 0;

		case eGraphicCaps_PolygonOffset:		return 1;	//OpenGL always support it!

		case eGraphicCaps_ShaderModel_2:		return (GLEW_ARB_fragment_program || GLEW_ARB_fragment_shader) ? 1 : 0;	//Mac always support this, so not a good test.
#ifdef __APPLE__
		case eGraphicCaps_ShaderModel_3:		return 0; // Force return false for OS X as dynamic branching doesn't work well (it's slow)
		case eGraphicCaps_ShaderModel_4:		return 0;
#else
		case eGraphicCaps_ShaderModel_3:
			{
				if(mbForceShaderModel3And4Off)
					return 0;
				else
					return  (GLEW_NV_vertex_program3 || GLEW_ATI_shader_texture_lod) ? 1 : 0;
			}
		case eGraphicCaps_ShaderModel_4:
			{
				if(mbForceShaderModel3And4Off)
					return 0;
				else
					return  GLEW_EXT_gpu_shader4 ? 1 : 0;
			}
#endif

		case eGraphicCaps_OGL_ATIFragmentShader: return  GLEW_ATI_fragment_shader ? 1 : 0;

		case eGraphicCaps_MaxColorRenderTargets:
			{
				GLint lMax;
				glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &lMax);
				return lMax;
			}
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ShowCursor(bool abX)
	{
		if(abX)
			SDL_ShowCursor(SDL_ENABLE);
		else
			SDL_ShowCursor(SDL_DISABLE);
	}

	//-----------------------------------------------------------------------

    void cLowLevelGraphicsSDL::SetWindowGrab(bool abX)
    {
        mbGrab = abX;
        if (hpl::entry_sdl::getWindow()) {
            SDL_SetWindowGrab(hpl::entry_sdl::getWindow(), abX ? SDL_TRUE : SDL_FALSE);
        }
    }

	void cLowLevelGraphicsSDL::SetRelativeMouse(bool abX)
	{
		SDL_SetRelativeMouseMode(abX ? SDL_TRUE : SDL_FALSE);
	}

    void cLowLevelGraphicsSDL::SetWindowCaption(const tString &asName)
    {
        SDL_SetWindowTitle(hpl::entry_sdl::getWindow(), asName.c_str());
    }

    bool cLowLevelGraphicsSDL::GetWindowMouseFocus()
    {
		return (hpl::entry_sdl::getWindowFlags()  & SDL_WINDOW_MOUSE_FOCUS) > 0;
    }

    bool cLowLevelGraphicsSDL::GetWindowInputFocus()
    {
		return (hpl::entry_sdl::getWindowFlags()  & SDL_WINDOW_INPUT_FOCUS) > 0;
    }

    bool cLowLevelGraphicsSDL::GetWindowIsVisible()
    {
		return (hpl::entry_sdl::getWindowFlags()  & SDL_WINDOW_SHOWN) > 0;
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
        SDL_SetWindowBrightness(hpl::entry_sdl::getWindow(), mfGammaCorrection);
	}

	float cLowLevelGraphicsSDL::GetGammaCorrection()
	{
		return mfGammaCorrection;
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetScreenSizeFloat()
	{
		const auto size = hpl::entry_sdl::getSize();
		return cVector2f(static_cast<float>(size.x), static_cast<float>(size.y));
	}

	const cVector2l cLowLevelGraphicsSDL::GetScreenSizeInt()
	{
		return hpl::entry_sdl::getSize();
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

	//-----------------------------------------------------------------------

	iGpuProgram* cLowLevelGraphicsSDL::CreateGpuProgram(const tString& asName)
	{
		;

		return hplNew( cGLSLProgram, (asName) );
		//return hplNew( cCGProgram, () );
	}

	iGpuShader* cLowLevelGraphicsSDL::CreateGpuShader(const tString& asName, eGpuShaderType aType)
	{
		;

		return hplNew( cGLSLShader, (asName,aType, this) );
		//return hplNew( cCGShader, (asName,mCG_Context, aType) );
	}

	//-----------------------------------------------------------------------

	iTexture* cLowLevelGraphicsSDL::CreateTexture(const tString &asName,eTextureType aType,   eTextureUsage aUsage)
	{
		;

		cSDLTexture *pTexture = hplNew( cSDLTexture, (asName,aType, aUsage, this) );

		return pTexture;
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
		;

		if(GetCaps(eGraphicCaps_RenderToTexture)==0) return NULL;

		return hplNew(cFrameBufferGL,(asName, this));
	}

	//-----------------------------------------------------------------------

	iDepthStencilBuffer* cLowLevelGraphicsSDL::CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits)
	{
		;

		if(GetCaps(eGraphicCaps_RenderToTexture)==0) return NULL;

		return hplNew(cDepthStencilBufferGL,(avSize, alDepthBits,alStencilBits));
	}

	//-----------------------------------------------------------------------

	iOcclusionQuery* cLowLevelGraphicsSDL::CreateOcclusionQuery()
	{
		;

		return hplNew(cOcclusionQueryOGL, () );
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ClearFrameBuffer(tClearFrameBufferFlag aFlags)
	{
		;

		GLbitfield bitmask=0;

		if(aFlags & eClearFrameBufferFlag_Color)	bitmask |= GL_COLOR_BUFFER_BIT;
		if(aFlags & eClearFrameBufferFlag_Depth)	bitmask |= GL_DEPTH_BUFFER_BIT;
		if(aFlags & eClearFrameBufferFlag_Stencil)	bitmask |= GL_STENCIL_BUFFER_BIT;

		glClear(bitmask);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetClearColor(const cColor& aCol){

	}
	void cLowLevelGraphicsSDL::SetClearDepth(float afDepth){

	}
	void cLowLevelGraphicsSDL::SetClearStencil(int alVal){

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
	}

	void cLowLevelGraphicsSDL::WaitAndFinishRendering()
	{
	}

	void cLowLevelGraphicsSDL::FlushRendering()
	{
	}

	void cLowLevelGraphicsSDL::SwapBuffers()
	{

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

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	GLenum GetGLWrapEnum(eTextureWrap aMode)
	{
		;

		switch(aMode)
		{
		case eTextureWrap_Clamp: return GL_CLAMP;
		case eTextureWrap_Repeat: return GL_REPEAT;
		case eTextureWrap_ClampToEdge: return GL_CLAMP_TO_EDGE;
		case eTextureWrap_ClampToBorder: return GL_CLAMP_TO_BORDER;
		}

		return GL_REPEAT;
	}


	//-----------------------------------------------------------------------

	GLenum PixelFormatToGLFormat(ePixelFormat aFormat)
	{
		;

		switch(aFormat)
		{
		case ePixelFormat_Alpha:			return GL_ALPHA;
		case ePixelFormat_Luminance:		return GL_LUMINANCE;
		case ePixelFormat_LuminanceAlpha:	return GL_LUMINANCE_ALPHA;
		case ePixelFormat_RGB:				return GL_RGB;
		case ePixelFormat_RGBA:				return GL_RGBA;
		case ePixelFormat_BGR:				return GL_BGR_EXT;
		case ePixelFormat_BGRA:				return GL_BGRA_EXT;
		case ePixelFormat_Depth16:			return GL_DEPTH_COMPONENT;
		case ePixelFormat_Depth24:			return GL_DEPTH_COMPONENT;
		case ePixelFormat_Depth32:			return GL_DEPTH_COMPONENT;
		case ePixelFormat_Alpha16:			return GL_ALPHA;
		case ePixelFormat_Luminance16:		return GL_LUMINANCE;
		case ePixelFormat_LuminanceAlpha16:	return GL_LUMINANCE_ALPHA;
		case ePixelFormat_RGB16:			return GL_RGB;
		case ePixelFormat_RGBA16:			return GL_RGBA;
		case ePixelFormat_Alpha32:			return GL_ALPHA;
		case ePixelFormat_Luminance32:		return GL_LUMINANCE;
		case ePixelFormat_LuminanceAlpha32: return GL_LUMINANCE_ALPHA;
		case ePixelFormat_RGB32:			return GL_RGB;
		case ePixelFormat_RGBA32:			return GL_RGBA;
		};

		return 0;
	}

	//-------------------------------------------------

	GLenum PixelFormatToGLInternalFormat(ePixelFormat aFormat)
	{
		;

		switch(aFormat)
		{
		case ePixelFormat_Alpha:			return GL_ALPHA;
		case ePixelFormat_Luminance:		return GL_LUMINANCE;
		case ePixelFormat_LuminanceAlpha:	return GL_LUMINANCE_ALPHA;
		case ePixelFormat_RGB:				return GL_RGB;
		case ePixelFormat_RGBA:				return GL_RGBA;
		case ePixelFormat_BGR:				return GL_RGB;
		case ePixelFormat_BGRA:				return GL_RGBA;
		case ePixelFormat_Depth16:			return GL_DEPTH_COMPONENT16;
		case ePixelFormat_Depth24:			return GL_DEPTH_COMPONENT24;
		case ePixelFormat_Depth32:			return GL_DEPTH_COMPONENT32;
		case ePixelFormat_Alpha16:			return GL_ALPHA16F_ARB;
		case ePixelFormat_Luminance16:		return GL_LUMINANCE16F_ARB;
		case ePixelFormat_LuminanceAlpha16:	return GL_LUMINANCE_ALPHA16F_ARB;
		case ePixelFormat_RGB16:			return GL_RGB16F_ARB;
		case ePixelFormat_RGBA16:			return GL_RGBA16F_ARB;
		case ePixelFormat_Alpha32:			return GL_ALPHA32F_ARB;
		case ePixelFormat_Luminance32:		return GL_LUMINANCE32F_ARB;
		case ePixelFormat_LuminanceAlpha32: return GL_LUMINANCE_ALPHA32F_ARB;
		case ePixelFormat_RGB32:			return GL_RGB32F_ARB;
		case ePixelFormat_RGBA32:			return GL_RGBA32F_ARB;
		};

		return 0;
	}

	//-------------------------------------------------

	GLenum GetGLCompressionFormatFromPixelFormat(ePixelFormat aFormat)
	{
		;

		switch(aFormat)
		{
		case ePixelFormat_DXT1:				return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		case ePixelFormat_DXT3:				return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		case ePixelFormat_DXT5:				return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		}
		return 0;
	}

	//-------------------------------------------------

	GLenum TextureTypeToGLTarget(eTextureType aType)
	{
		switch(aType)
		{
		case eTextureType_1D:		return GL_TEXTURE_1D;
		case eTextureType_2D:		return GL_TEXTURE_2D;
		case eTextureType_Rect:		return GL_TEXTURE_RECTANGLE_NV;
		case eTextureType_CubeMap:	return GL_TEXTURE_CUBE_MAP_ARB;
		case eTextureType_3D:		return GL_TEXTURE_3D;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLBlendEnum(eBlendFunc aType)
	{
		;

		switch(aType)
		{
		case eBlendFunc_Zero:					return GL_ZERO;
		case eBlendFunc_One:					return GL_ONE;
		case eBlendFunc_SrcColor:				return GL_SRC_COLOR;
		case eBlendFunc_OneMinusSrcColor:		return GL_ONE_MINUS_SRC_COLOR;
		case eBlendFunc_DestColor:				return GL_DST_COLOR;
		case eBlendFunc_OneMinusDestColor:		return GL_ONE_MINUS_DST_COLOR;
		case eBlendFunc_SrcAlpha:				return GL_SRC_ALPHA;
		case eBlendFunc_OneMinusSrcAlpha:		return GL_ONE_MINUS_SRC_ALPHA;
		case eBlendFunc_DestAlpha:				return GL_DST_ALPHA;
		case eBlendFunc_OneMinusDestAlpha:		return GL_ONE_MINUS_DST_ALPHA;
		case eBlendFunc_SrcAlphaSaturate:		return GL_SRC_ALPHA_SATURATE;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureParamEnum(eTextureParam aType)
	{
		;

		switch(aType)
		{
		case eTextureParam_ColorFunc:		return GL_COMBINE_RGB_ARB;
		case eTextureParam_AlphaFunc:		return GL_COMBINE_ALPHA_ARB;
		case eTextureParam_ColorSource0:	return GL_SOURCE0_RGB_ARB;
		case eTextureParam_ColorSource1:	return GL_SOURCE1_RGB_ARB;
		case eTextureParam_ColorSource2:	return GL_SOURCE2_RGB_ARB;
		case eTextureParam_AlphaSource0:	return GL_SOURCE0_ALPHA_ARB;
		case eTextureParam_AlphaSource1:	return GL_SOURCE1_ALPHA_ARB;
		case eTextureParam_AlphaSource2:	return GL_SOURCE2_ALPHA_ARB;
		case eTextureParam_ColorOp0:		return GL_OPERAND0_RGB_ARB;
		case eTextureParam_ColorOp1:		return GL_OPERAND1_RGB_ARB;
		case eTextureParam_ColorOp2:		return GL_OPERAND2_RGB_ARB;
		case eTextureParam_AlphaOp0:		return GL_OPERAND0_ALPHA_ARB;
		case eTextureParam_AlphaOp1:		return GL_OPERAND1_ALPHA_ARB;
		case eTextureParam_AlphaOp2:		return GL_OPERAND2_ALPHA_ARB;
		case eTextureParam_ColorScale:		return GL_RGB_SCALE_ARB;
		case eTextureParam_AlphaScale:		return GL_ALPHA_SCALE;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureOpEnum(eTextureOp aType)
	{
		;

		switch(aType)
		{
		case eTextureOp_Color:			return GL_SRC_COLOR;
		case eTextureOp_OneMinusColor:	return GL_ONE_MINUS_SRC_COLOR;
		case eTextureOp_Alpha:			return GL_SRC_ALPHA;
		case eTextureOp_OneMinusAlpha:	return GL_ONE_MINUS_SRC_ALPHA;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureSourceEnum(eTextureSource aType)
	{
		;

		switch(aType)
		{
		case eTextureSource_Texture:	return GL_TEXTURE;
		case eTextureSource_Constant:	return GL_CONSTANT_ARB;
		case eTextureSource_Primary:	return GL_PRIMARY_COLOR_ARB;
		case eTextureSource_Previous:	return GL_PREVIOUS_ARB;
		}
		return 0;
	}
	//-----------------------------------------------------------------------

	GLenum GetGLTextureTargetEnum(eTextureType aType)
	{
		;

		switch(aType)
		{
		case eTextureType_1D:		return GL_TEXTURE_1D;
		case eTextureType_2D:		return GL_TEXTURE_2D;
		case eTextureType_Rect:
			{
				return GL_TEXTURE_RECTANGLE_NV;
			}
		case eTextureType_CubeMap:	return GL_TEXTURE_CUBE_MAP_ARB;
		case eTextureType_3D:		return GL_TEXTURE_3D;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureCompareMode(eTextureCompareMode aMode)
	{
		;

		switch(aMode)
		{
		case eTextureCompareMode_None:			return GL_NONE;
		case eTextureCompareMode_RToTexture:	return GL_COMPARE_R_TO_TEXTURE;
		}
		return 0;
	}

	GLenum GetGLTextureCompareFunc(eTextureCompareFunc aFunc)
	{
		switch(aFunc)
		{
		case eTextureCompareFunc_LessOrEqual:		return GL_LEQUAL;
		case eTextureCompareFunc_GreaterOrEqual:	return GL_GEQUAL;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureFuncEnum(eTextureFunc aType)
	{
		;

		switch(aType)
		{
		case eTextureFunc_Modulate:		return GL_MODULATE;
		case eTextureFunc_Replace:		return GL_REPLACE;
		case eTextureFunc_Add:			return GL_ADD;
		case eTextureFunc_Substract:	return GL_SUBTRACT_ARB;
		case eTextureFunc_AddSigned:	return GL_ADD_SIGNED_ARB;
		case eTextureFunc_Interpolate:	return GL_INTERPOLATE_ARB;
		case eTextureFunc_Dot3RGB:		return GL_DOT3_RGB_ARB;
		case eTextureFunc_Dot3RGBA:		return GL_DOT3_RGBA_ARB;
		}
		return 0;
	}

	//-----------------------------------------------------------------------
	GLenum GetGLDepthTestFuncEnum(eDepthTestFunc aType)
	{
		;

		switch(aType)
		{
		case eDepthTestFunc_Never:			return GL_NEVER;
		case eDepthTestFunc_Less:				return GL_LESS;
		case eDepthTestFunc_LessOrEqual:		return GL_LEQUAL;
		case eDepthTestFunc_Greater:			return GL_GREATER;
		case eDepthTestFunc_GreaterOrEqual:	return GL_GEQUAL;
		case eDepthTestFunc_Equal:			return GL_EQUAL;
		case eDepthTestFunc_NotEqual:			return GL_NOTEQUAL;
		case eDepthTestFunc_Always:			return GL_ALWAYS;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLAlphaTestFuncEnum(eAlphaTestFunc aType)
	{
		;

		switch(aType)
		{
		case eAlphaTestFunc_Never:			return GL_NEVER;
		case eAlphaTestFunc_Less:				return GL_LESS;
		case eAlphaTestFunc_LessOrEqual:		return GL_LEQUAL;
		case eAlphaTestFunc_Greater:			return GL_GREATER;
		case eAlphaTestFunc_GreaterOrEqual:	return GL_GEQUAL;
		case eAlphaTestFunc_Equal:			return GL_EQUAL;
		case eAlphaTestFunc_NotEqual:			return GL_NOTEQUAL;
		case eAlphaTestFunc_Always:			return GL_ALWAYS;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLStencilFuncEnum(eStencilFunc aType)
	{
		;

		switch(aType)
		{
		case eStencilFunc_Never:			return GL_NEVER;
		case eStencilFunc_Less:				return GL_LESS;
		case eStencilFunc_LessOrEqual:		return GL_LEQUAL;
		case eStencilFunc_Greater:			return GL_GREATER;
		case eStencilFunc_GreaterOrEqual:	return GL_GEQUAL;
		case eStencilFunc_Equal:			return GL_EQUAL;
		case eStencilFunc_NotEqual:			return GL_NOTEQUAL;
		case eStencilFunc_Always:			return GL_ALWAYS;
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	GLenum GetGLStencilOpEnum(eStencilOp aType)
	{
		;

		switch(aType)
		{
		case eStencilOp_Keep:			return GL_KEEP;
		case eStencilOp_Zero:			return GL_ZERO;
		case eStencilOp_Replace:		return GL_REPLACE;
		case eStencilOp_Increment:		return GL_INCR;
		case eStencilOp_Decrement:		return GL_DECR;
		case eStencilOp_Invert:			return GL_INVERT;
		case eStencilOp_IncrementWrap:	return GL_INCR_WRAP_EXT;
		case eStencilOp_DecrementWrap:	return GL_DECR_WRAP_EXT;
		}
		return 0;
	}

	//-----------------------------------------------------------------------



}
