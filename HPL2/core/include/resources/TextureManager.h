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

#include <graphics/Image.h>
#include "resources/ResourceManager.h"
#include "graphics/Texture.h"
#include <functional>

namespace hpl {

	class cGraphics;
	class cResources;
	class iTexture;
	class cBitmapLoaderHandler;
	//------------------------------------------------------

	typedef std::map<tString, iTexture*> tTextureAttenuationMap;
	typedef std::map<tString, iTexture*>::iterator tTextureAttenuationMapIt;

	//------------------------------------------------------

	class cTextureManager : public iResourceManager
	{
	public:
		cTextureManager(cGraphics* apGraphics,cResources *apResources);
		~cTextureManager();


		Image* Create1DImage(const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
							unsigned int alTextureSizeLevel=0);

		Image* Create2DImage(const tString& asName,bool abUseMipMaps,eTextureType aType= eTextureType_2D,
							eTextureUsage aUsage=eTextureUsage_Normal,unsigned int alTextureSizeLevel=0);

		Image* Create3DImage(const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
							unsigned int alTextureSizeLevel=0);
		
		Image* CreateCubeMapImage(const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
					unsigned int alTextureSizeLevel=0);


		[[deprecated("Use Create1DImage instead")]]
		iTexture* Create1D(	const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
							unsigned int alTextureSizeLevel=0);
		[[deprecated("Use Create2DImage instead")]]
		iTexture* Create2D(	const tString& asName,bool abUseMipMaps,eTextureType aType= eTextureType_2D,
							eTextureUsage aUsage=eTextureUsage_Normal,unsigned int alTextureSizeLevel=0);
		[[deprecated("Use Create3DImage instead")]]
		iTexture* Create3D(	const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
							unsigned int alTextureSizeLevel=0);

		/**
		 * Creates an animated texture. The name must be [name]01.[ext]. And then the textures in the animation must
		 * be named [name]01.[ext], [name]02.[ext], etc
		 */
		iTexture* CreateAnim(	const tString& asFirstFrameName,bool abUseMipMaps, eTextureType aType,
								eTextureUsage aUsage=eTextureUsage_Normal,
								unsigned int alTextureSizeLevel=0);

		iTexture* CreateCubeMap(const tString& asName,bool abUseMipMaps, eTextureUsage aUsage=eTextureUsage_Normal,
								unsigned int alTextureSizeLevel=0);


		void Destroy(iResourceBase* apResource);
		void Unload(iResourceBase* apResource);

		void Update(float afTimeStep);

		int GetMemoryUsage(){ return mlMemoryUsage;}

	private:

		Image* _wrapperImageResource(const tString& asName, std::function<Image*(const tString& asName, const tWString& path, cBitmap* bitmap)> createImageHandler);

		iTexture* FindTexture2D(const tString &asName, tWString &asFilePath);
		Image* FindImageResource(const tString &asName, tWString &asFilePath);

		tTextureAttenuationMap m_mapAttenuationTextures;

		tStringVec mvCubeSideSuffixes;

		int mlMemoryUsage;

		cGraphics* mpGraphics;
		cResources* mpResources;
		cBitmapLoaderHandler *mpBitmapLoaderHandler;
	};

};