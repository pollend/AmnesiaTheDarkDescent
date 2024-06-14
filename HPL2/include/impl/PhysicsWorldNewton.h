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

#ifndef HPL_PHYSICS_WORLD_NEWTON_H
#define HPL_PHYSICS_WORLD_NEWTON_H

#include "physics/PhysicsWorld.h"

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <Newton.h>

namespace hpl {
	class cPhysicsWorldNewton : public iPhysicsWorld
	{
	public:
		cPhysicsWorldNewton();
		~cPhysicsWorldNewton();

		void Simulate(float afTimeStep) override;

		void  SetMaxTimeStep(float afTimeStep) override;
		float GetMaxTimeStep() override;

		void SetWorldSize(const cVector3f &avMin,const cVector3f &avMax) override;
		cVector3f GetWorldSizeMin() override;
		cVector3f GetWorldSizeMax() override;

		void SetGravity(const cVector3f& avGravity) override;
		cVector3f GetGravity() override;

		void SetAccuracyLevel(ePhysicsAccuracy aAccuracy) override;
		ePhysicsAccuracy GetAccuracyLevel() override;

		void SetNumberOfThreads(int alThreads) override;
		int GetNumberOfThreads() override;

		iCollideShape* CreateNullShape() override;
		iCollideShape* CreateBoxShape(const cVector3f &avSize, cMatrixf* apOffsetMtx) override;
		iCollideShape* CreateSphereShape(const cVector3f &avRadii, cMatrixf* apOffsetMtx) override;
		iCollideShape* CreateCylinderShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx) override;
		iCollideShape* CreateCapsuleShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx) override;

		iCollideShape* CreateMeshShape(iVertexBuffer *apVtxBuffer) override;
	    iCollideShape* CreateMeshShape(uint32_t numberIndecies,
		                uint32_t vertexOffset, uint32_t indexOffset,
		                GraphicsBuffer::BufferIndexView indexView,
		                GraphicsBuffer::BufferStructuredView<float3> position) override;

	    iCollideShape* LoadMeshShapeFromBuffer(cBinaryBuffer *apBuffer) override;
		void SaveMeshShapeToBuffer(iCollideShape* apMeshShape, cBinaryBuffer *apBuffer) override;

		iCollideShape* CreateCompundShape(tCollideShapeVec &avShapes) override;
		iCollideShape* CreateStaticSceneShape(tCollideShapeVec &avShapes, tMatrixfVec *apMatrices) override;

		iPhysicsJointBall* CreateJointBall(const tString &asName,const cVector3f& avPivotPoint,
												const cVector3f& avPinDir,
												iPhysicsBody* apParentBody, iPhysicsBody *apChildBody) override;
		iPhysicsJointHinge* CreateJointHinge(const tString &asName,const cVector3f& avPivotPoint,
												const cVector3f& avPinDir,
												iPhysicsBody* apParentBody, iPhysicsBody *apChildBody) override;
		iPhysicsJointSlider* CreateJointSlider(const tString &asName,const cVector3f& avPivotPoint,
												const cVector3f& avPinDir,
												iPhysicsBody* apParentBody, iPhysicsBody *apChildBody) override;
		iPhysicsJointScrew* CreateJointScrew(const tString &asName,const cVector3f& avPivotPoint,
												const cVector3f& avPinDir,
												iPhysicsBody* apParentBody, iPhysicsBody *apChildBody) override;

		iPhysicsBody* CreateBody(const tString &asName,iCollideShape *apShape) override;

		void GetBodiesInBV(cBoundingVolume *apBV, std::vector<iPhysicsBody*> *apBodyVec) override;

		iCharacterBody *CreateCharacterBody(const tString &asName, const cVector3f &avSize) override;

		iPhysicsMaterial* CreateMaterial(const tString &asName) override;

		iPhysicsController *CreateController(const tString &asName) override;

		iPhysicsRope* CreateRope(const tString &asName, const cVector3f &avStartPos, const cVector3f &avEndPos) override;

		void CastRay(iPhysicsRayCallback *apCallback,
							const cVector3f &avOrigin, const cVector3f& avEnd,
							bool abCalcDist, bool abCalcNormal, bool abCalcPoint,
							bool abUsePrefilter = false) override;

		bool CheckShapeCollision(	iCollideShape* apShapeA, const cMatrixf& a_mtxA,
						iCollideShape* apShapeB, const cMatrixf& a_mtxB,
						cCollideData & aCollideData, int alMaxPoints,
						bool abCorrectNormalDirection) override;

		void RenderShapeDebugGeometry(	iCollideShape *apShape, const cMatrixf& a_mtxTransform,
										DebugDraw *apLowLevel, const cColor& aColor) override;
		void RenderDebugGeometry(DebugDraw *apLowLevel, const cColor& aColor) override;

		NewtonWorld* GetNewtonWorld(){ return mpNewtonWorld;}
	private:
		NewtonWorld *mpNewtonWorld;

		float* mpTempPoints;
		float* mpTempNormals;
		float* mpTempDepths;

		cVector3f mvWorldSizeMin;
		cVector3f mvWorldSizeMax;
		cVector3f mvGravity;
		float mfMaxTimeStep;

		ePhysicsAccuracy mAccuracy;
	};
};
#endif // HPL_PHYSICS_WORLD_NEWTON_H
