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

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/Entity3D.h"
#include "graphics/Renderable.h"

namespace hpl {

    class cMaterialManager;
    class cResources;
    class cGraphics;
    class cFileSearcher;
    class iLowLevelGraphics;
    class cMaterial;
    class iVertexBuffer;

    class cBeam;
    class cBeamEnd : public iEntity3D {
        friend class cBeam;
        friend class cBeamEnd_UpdateCallback;

    public:
        cBeamEnd(const tString asName, cBeam* apBeam)
            : iEntity3D(asName)
            , mColor(1, 1)
            , mpBeam(apBeam) {
        }

        void SetColor(const cColor& aColor);
        const cColor& GetColor() {
            return mColor;
        }

        /////////////////////////////////
        // Entity implementation
        tString GetEntityType() {
            return "BeamEnd";
        }

    private:
        cColor mColor;
        cBeam* mpBeam;
    };

    //------------------------------------------

    class cBeamEnd_UpdateCallback : public iEntityCallback {
    public:
        void OnTransformUpdate(iEntity3D* apEntity);
    };

    //------------------------------------------

    class cBeam : public iRenderable {
        friend class cBeamEnd;

    public:
        cBeam(const tString asName, cResources* apResources, cGraphics* apGraphics);
        ~cBeam();

        void SetMaterial(cMaterial* apMaterial);

        const tString& GetFileName() {
            return msFileName;
        }

        /**
         * Set the size. X= the thickness of the line, width of texture used. Y = the length that one texture height takes.
         * \param avSize
         */
        void SetSize(const cVector2f& avSize);
        cVector2f GetSize() {
            return mvSize;
        }

        void SetColor(const cColor& aColor);
        const cColor& GetColor() {
            return mColor;
        }

        void SetTileHeight(bool abX);
        bool GetTileHeight() {
            return mbTileHeight;
        }

        void SetMultiplyAlphaWithColor(bool abX);
        bool GetMultiplyAlphaWithColor() {
            return mbMultiplyAlphaWithColor;
        }

        cBeamEnd* GetEnd() {
            return mpEnd;
        }

        bool LoadXMLProperties(const tString asFile);

        cVector3f GetAxis() {
            return mvAxis;
        }
        cVector3f GetMidPosition() {
            return mvMidPosition;
        }

        // Entity implementation
        tString GetEntityType() {
            return "Beam";
        }

        bool IsVisible();

        // Renderable implementations
        virtual cMaterial* GetMaterial() override {
            return mpMaterial;
        }
        virtual iVertexBuffer* GetVertexBuffer() override {
            return nullptr;
        }
        virtual DrawPacket ResolveDrawPacket(const ForgeRenderer::Frame& frame) override;

        void UpdateGraphicsForFrame(float afFrameTime);

        cBoundingVolume* GetBoundingVolume();

        cMatrixf* GetModelMatrix(cFrustum* apFrustum);

        int GetMatrixUpdateCount();

        eRenderableType GetRenderType() {
            return eRenderableType_Beam;
        }

    private:
        std::shared_ptr<GeometrySet::GeometrySetSubAllocation> m_geometry;

        cMaterialManager* mpMaterialManager;
        cFileSearcher* mpFileSearcher;
        iLowLevelGraphics* mpLowLevelGraphics;

        cMaterial* mpMaterial;
        cBeamEnd* mpEnd;

        tString msFileName;

        int mlStartTransformCount;
        int mlEndTransformCount;

        cMatrixf m_mtxTempTransform;

        int mlLastRenderCount;

        cBeamEnd_UpdateCallback mEndCallback;

        cVector2f mvSize;

        cVector3f mvAxis;
        cVector3f mvMidPosition;

        bool mbTileHeight;
        bool mbMultiplyAlphaWithColor;

        cColor mColor = cColor(1,1,1,1);
        uint8_t m_activeCopy = 0;
    };

}; // namespace hpl
