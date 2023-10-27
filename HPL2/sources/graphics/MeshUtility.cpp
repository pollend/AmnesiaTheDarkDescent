#include "graphics/MeshUtility.h"
#include "Common_3/Utilities/Log/Log.h"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/sse/vectormath.hpp"
#include "graphics/GraphicsBuffer.h"
#include "graphics/mikktspace.h"
#include "math/Math.h"

#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

#include <cfloat>
#include <cstdint>
#include <folly/small_vector.h>
#include <variant>

namespace hpl::MeshUtility {
    namespace details {
        struct MikktSpaceUser {
            uint32_t m_numElements;
            uint32_t m_numIndices;
            GraphicsBuffer::BufferIndexView* m_indexView;
            GraphicsBuffer::BufferStructuredView<float3>* m_position;
            GraphicsBuffer::BufferStructuredView<float2>* m_uv;
            GraphicsBuffer::BufferStructuredView<float3>* m_normal;
            GraphicsBuffer::BufferStructuredView<float3>* m_tangent;
        };

        inline void CreateTriTangentVectorsHPL2(
            uint32_t numVerts,
            uint32_t numIndecies,
            GraphicsBuffer::BufferIndexView* indexView,
            GraphicsBuffer::BufferStructuredView<float3>* position,
            GraphicsBuffer::BufferStructuredView<float2>* uv,
            GraphicsBuffer::BufferStructuredView<float3>* normal,
            std::function<void(uint32_t, float4)> handler
        ) {

        }


        uint32_t WrapSides(uint32_t viewIndex, GraphicsBuffer::BufferIndexView* view, int alStartVertexIdx, int alSections) {
            // Create indices  like this		0 --- 1 --- 2 --- ... --- 0
            //									|   / |   / |
            //									|  /  |  /  |
            //									| /	  | /   |
            //									Se---Se+1---Se+2---2S-1--- S

            for (int i = 0; i < alSections - 1; ++i) {
                int lPoint0 = alStartVertexIdx + i;
                int lPoint1 = lPoint0 + 1;
                int lPoint2 = lPoint0 + alSections;
                int lPoint3 = lPoint2 + 1;

                view->Write(viewIndex++, lPoint0);
                view->Write(viewIndex++, lPoint2);
                view->Write(viewIndex++, lPoint1);

                view->Write(viewIndex++, lPoint1);
                view->Write(viewIndex++, lPoint2);
                view->Write(viewIndex++, lPoint3);
            }

            {
                int lPoint0 = alStartVertexIdx + alSections - 1;
                int lPoint1 = alStartVertexIdx;
                int lPoint2 = lPoint0 + alSections;
                int lPoint3 = lPoint0 + 1;

                view->Write(viewIndex++, lPoint0);
                view->Write(viewIndex++, lPoint2);
                view->Write(viewIndex++, lPoint1);

                view->Write(viewIndex++, lPoint1);
                view->Write(viewIndex++, lPoint2);
                view->Write(viewIndex++, lPoint3);

                // Log("%d %d %d\t %d %d %d\n", lPoint0, lPoint2, lPoint1,
                //						lPoint1, lPoint2, lPoint3);
            }
            return viewIndex;
        }
        void CreateCircumference(float afRadius, float afAngleStep, float afHeight, std::function<void(float3)> handler) {
            float fEnd = k2Pif - afAngleStep;
            for (float fAngle = 0; fAngle <= fEnd; fAngle += afAngleStep) {
                float3 vPoint = float3(
                    cMath::RoundFloatToDecimals(afRadius * cos(fAngle), 6),
                    cMath::RoundFloatToDecimals(afHeight, 6),
                    cMath::RoundFloatToDecimals(afRadius * sin(fAngle), 6));
                handler(vPoint);
            }
        }
        uint32_t WrapLowerCap(
            uint32_t viewIndex, GraphicsBuffer::BufferIndexView* view, int alCenterVertexIdx, int alStartVertexIdx, int alSections) {
            for (int i = 0; i < alSections - 1; ++i) {
                int lBase = alStartVertexIdx + i;
                view->Write(viewIndex++, alCenterVertexIdx);
                view->Write(viewIndex++, lBase + 1);
                view->Write(viewIndex++, lBase);
            }

            view->Write(viewIndex++, alCenterVertexIdx);
            view->Write(viewIndex++, alStartVertexIdx);
            view->Write(viewIndex++, alStartVertexIdx + alSections - 1);
            return viewIndex;
        }
        uint32_t WrapUpperCap(
            uint32_t viewIndex, GraphicsBuffer::BufferIndexView* view, int alCenterVertexIdx, int alStartVertexIdx, int alSections) {
            for (int i = 0; i < alSections - 1; ++i) {
                int lBase = alStartVertexIdx + i;
                view->Write(viewIndex++, alCenterVertexIdx);
                view->Write(viewIndex++, lBase);
                view->Write(viewIndex++, lBase + 1);
            }
            view->Write(viewIndex++, alCenterVertexIdx);
            view->Write(viewIndex++, alStartVertexIdx + alSections - 1);
            view->Write(viewIndex++, alStartVertexIdx);
            return viewIndex;
        }

        int mikkT_getNumberFaces(const SMikkTSpaceContext* pContext) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            return user->m_numIndices / 3;
        }

        int mikkT_getNumVerticesOfFaces(const SMikkTSpaceContext* pContext, const int iFace) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            return 3;
        }

        void mikkT_getPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float3 v = user->m_position->Get(vertIdx);

            fvPosOut[0] = v.getX();
            fvPosOut[1] = v.getY();
            fvPosOut[2] = v.getZ();
        }
        void mikkT_getNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float3 v = user->m_normal->Get(vertIdx);

            fvNormOut[0] = v.getX();
            fvNormOut[1] = v.getY();
            fvNormOut[2] = v.getZ();
        }
        void mikkT_getTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float2 v = user->m_uv->Get(vertIdx);

            fvTexcOut[0] = v.getX();
            fvTexcOut[1] = v.getY();
        }
        void mikkT_setTSpaceBasic(
            const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            user->m_tangent->Write(vertIdx, float3(fvTangent[0], fvTangent[1], fvTangent[2]));
        }
    } // namespace details

    void Transform(
        uint32_t numElements,
        const Matrix4& transform,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float3>* tangent) {
        Matrix3 normalMat = transpose(inverse(transform.getUpper3x3()));
        for (size_t i = 0; i < numElements; i++) {
            if (position) {
                float3 value = position->Get(i);
                position->Write(i, v3ToF3((transform * Vector4(f3Tov3(value), 1.0f)).getXYZ()));
            }
            if (normal) {
                float3 value = normal->Get(i);
                normal->Write(i, v3ToF3(normalMat * f3Tov3(value)));
            }
            if (tangent) {
                float3 value = tangent->Get(i);
                tangent->Write(i, v3ToF3(normalMat * f3Tov3(value)));
            }
        }
    }

    void CreateTriTangentVectorsHPL2(
        uint32_t numVerts,
        uint32_t numIndices,
        GraphicsBuffer::BufferIndexView* indexView,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent
    ) {
            struct TangentPairs {
                float3 sDir;
                float3 tDir;
            };

            std::vector<TangentPairs> tangentPairs;
            tangentPairs.resize(numVerts, {
                float3(0, 0 ,0),
                float3(0, 0 ,0)
            });

            for(size_t idxBuffer = 0; idxBuffer < numIndices;  idxBuffer += 3) {
                const uint32_t vtxIdx1 = indexView->Get(idxBuffer + 0);
                const uint32_t vtxIdx2 = indexView->Get(idxBuffer + 1);
                const uint32_t vtxIdx3 = indexView->Get(idxBuffer + 2);

                const float3 pos1 = position->Get(vtxIdx1);
                const float3 pos2 = position->Get(vtxIdx2);
                const float3 pos3 = position->Get(vtxIdx3);

                const float2 tex1 = uv->Get(vtxIdx1);
                const float2 tex2 = uv->Get(vtxIdx2);
                const float2 tex3 = uv->Get(vtxIdx3);

		        //Get the vectors between the positions.
                const float3 vPos1To2 = pos2 - pos1;
			    const float3 vPos1To3 = pos3 - pos1;

		        //Get the vectors between the tex coords
			    const float2 vTex1To2 = tex2 - tex1;
			    const float2 vTex1To3 = tex3 - tex1;

		        const float fR = 1.0f / (vTex1To2.x * vTex1To3.y - vTex1To2.y * vTex1To3.x);

		        const float3 sDir((vTex1To3.y * vPos1To2.x - vTex1To2.y * vPos1To3.x) * fR,
							    (vTex1To3.y * vPos1To2.y - vTex1To2.y * vPos1To3.y) * fR,
							    (vTex1To3.y * vPos1To2.z - vTex1To2.y * vPos1To3.z) * fR
							    );

			    const float3 tDir((vTex1To2.x * vPos1To3.x - vTex1To3.x * vPos1To2.x) * fR,
							    (vTex1To2.x * vPos1To3.y - vTex1To3.x * vPos1To2.y) * fR,
							    (vTex1To2.x * vPos1To3.z - vTex1To3.x * vPos1To2.z) * fR
							    );

			    tangentPairs[vtxIdx1].sDir += sDir;
			    tangentPairs[vtxIdx2].sDir += sDir;
			    tangentPairs[vtxIdx3].sDir += sDir;

			    tangentPairs[vtxIdx1].tDir += tDir;
			    tangentPairs[vtxIdx2].tDir += tDir;
			    tangentPairs[vtxIdx3].tDir += tDir;
            }
		    const float fMaxCosAngle = -1.0f;
		    for(int i=0; i < numVerts; i++)
		    {
			    for(int j=i+1; j< numVerts; j++)
			    {
                    const float3 dp = (position->Get(i) - position->Get(j));
                    const float3 dn = (normal->Get(i) - normal->Get(j));
                    if(
                        (abs(dp.x) < FLT_EPSILON && abs(dp.y) < FLT_EPSILON && abs(dp.z) < FLT_EPSILON) &&
                        (abs(dn.x) < FLT_EPSILON && abs(dn.y) < FLT_EPSILON && abs(dn.z) < FLT_EPSILON)
                        ) {
                        if(dot(f3Tov3(tangentPairs[i].sDir), f3Tov3(tangentPairs[i].tDir)) >= fMaxCosAngle) {
                            float3 idir = tangentPairs[i].sDir;
                            float3 jdir = tangentPairs[j].sDir;
                            tangentPairs[j].sDir = idir ;
                            tangentPairs[i].sDir = jdir;
                        }

                        if(dot(f3Tov3(tangentPairs[i].sDir), f3Tov3(tangentPairs[i].tDir)) >= fMaxCosAngle) {
                            float3 idir = tangentPairs[i].tDir;
                            float3 jdir = tangentPairs[j].tDir;
                            tangentPairs[j].tDir = idir;
                            tangentPairs[i].tDir = jdir;
                        }
                    }
			    }
		    }

	        for(uint32_t vtxIdx = 0;vtxIdx < numVerts; vtxIdx++) {
	            Vector3 norm = f3Tov3(normal->Get(vtxIdx));
                auto& tangentPair = tangentPairs[vtxIdx];
                Vector3 tang = f3Tov3(tangentPair.sDir) - mulPerElem(norm, cross(norm, f3Tov3(tangentPair.sDir)));
                float fw = dot(cross(norm,f3Tov3(tangentPair.sDir)), f3Tov3(tangentPair.tDir)) < 0.0f ? -1.0f : 1.0f;
                std::visit([&](auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T,GraphicsBuffer::BufferStructuredView<float3>*>) {
                        item->Write(vtxIdx, v3ToF3(tang));
                    }
                    if constexpr (std::is_same_v<T,GraphicsBuffer::BufferStructuredView<float4>*>) {
                        item->Write(vtxIdx, float4(v3ToF3(tang), fw));
                    }
                }, tangent);
	        }
    }
    MeshCreateResult CreateSphere(
        float afRadius,
        uint32_t alSections,
        uint32_t alSlices,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent) {
        ASSERT(index);
        ASSERT(position);
        ASSERT(color);
        ASSERT(normal);
        ASSERT(uv);

        /////////////////////////////////
        // Set up variables
        const float fInvSlices = 1.0f / alSlices;
        const float fDiameter = 2 * afRadius;
        const float fHeightStep = fDiameter * fInvSlices;
        const float fSectionStep = k2Pif / (float)alSections;
        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;

        /////////////////////////////////
        // Create north pole vertex
        position->Write(vertexBufIdx, float3(0.0f, afRadius, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, 1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

        /////////////////////////////////
        // Create slice vertices
        const float fPiOverSlices = kPif * fInvSlices;
        const float fHalfPi = kPif * 0.5f;
        for (int i = 0; i < alSlices - 1; ++i) {
            const float fHeight = afRadius - afRadius * (1 + sin(fPiOverSlices * (float)(i + 1) - fHalfPi));
            const float fAngle = asin(fHeight / afRadius);
            details::CreateCircumference(afRadius * cos(fAngle), fSectionStep, fHeight, [&](float3 vertex) {
                position->Write(vertexBufIdx, vertex);
                normal->Write(vertexBufIdx, v3ToF3(normalize(f3Tov3(vertex))));
                color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
                uv->Write(vertexBufIdx++, float2(0, 0));
            });
        }
        /////////////////////////////////
        // Create south pole vertex
        position->Write(vertexBufIdx, float3(0.0f, -afRadius, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, -1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

        //////////////////////////////////
        // Create triangles (Wrap helpers do this for us)

        // Create north pole slice triangles
        indexBufIndex = details::WrapUpperCap(indexBufIndex, index, 0, 1, alSections);

        // Create faces for inner slices
        for (int i = 0; i < alSlices - 2; ++i) {
            indexBufIndex = details::WrapSides(indexBufIndex,index, 1 + i * alSections, alSections);
        }

        // Create triangles for south pole slice
        {
            const int lLastVertex = vertexBufIdx - 1;
            const int lSliceStart = lLastVertex - alSections;
            indexBufIndex = details::WrapLowerCap(indexBufIndex, index, lLastVertex, lSliceStart, alSections);
        }

        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices = indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }

    MeshCreateResult CreatePlane(
        Vector3 p1,
        Vector3 p2,
        Vector2 uv1,
        Vector2 uv2,
        Vector2 uv3,
        Vector2 uv4,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent
    ) {
        const Vector3 diff = p1 - p2;
        folly::small_vector<uint32_t, 4> vPlaneAxes;
        int lPlaneNormalAxis = -1;
        int lNumSameCoords = 0;
        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;
        for (int i = 0; i < 3; ++i) {
            if (diff[i] == 0) {
                lPlaneNormalAxis = i;
                ++lNumSameCoords;
            } else {
                vPlaneAxes.push_back(i);
            }
        }
        if (lPlaneNormalAxis < 0 || lNumSameCoords > 1) {
            LOGF(LogLevel::eERROR, "CreatePlane failed: plane corners are not coplanar");
            return MeshCreateResult{0};
        }
        Vector3 vCenter = (p1 + p2) * 0.5f;

        std::array<Vector3, 4> vTempCoords;
        vTempCoords[0] = p1;
        vTempCoords[1] = p2;
        Vector3 vTest1;
        Vector3 vTest2;

        for (int i = 2; i < 4; ++i) {
            int lIndex1 = vPlaneAxes[0];
            int lIndex2 = vPlaneAxes[1];

            vTempCoords[i][lPlaneNormalAxis] = vTempCoords[0][lPlaneNormalAxis];
            vTempCoords[i][lIndex1] = vTempCoords[i - 2][lIndex1];
            vTempCoords[i][lIndex2] = vTempCoords[3 - i][lIndex2];

            vTest1 = vTempCoords[2];
            vTest2 = vTempCoords[3];
        }

        std::array<Vector3, 4> vCoords;
        for (int i = 0; i < (int)vCoords.size(); ++i) {
            int lCornerIndex;
            Vector3 vCorner = vTempCoords[i] - vCenter;
            if (vCorner[vPlaneAxes[0]] <= 0) {
                if (vCorner[vPlaneAxes[1]] > 0) {
                    lCornerIndex = 0;
                } else {
                    lCornerIndex = 1;
                }
            } else {
                if (vCorner[vPlaneAxes[1]] > 0) {
                    lCornerIndex = 3;
                } else {
                    lCornerIndex = 2;
                }
            }
            vCoords[lCornerIndex] = vTempCoords[i];
        }
        Vector3 vFirstCorner = vCoords[1];
        for (int i = 0; i < (int)vCoords.size(); ++i) {
            vCoords[i] -= vFirstCorner;
        }
        std::array<Vector2, 4> uvs;
        uvs[0] = uv1;
        uvs[1] = uv2;
        uvs[2] = uv3;
        uvs[3] = uv4;
        for (size_t i = 0; i < 4; i++) {
            position->Write(vertexBufIdx, v3ToF3(vCoords[i]));
            normal->Write(vertexBufIdx, float3(0.0f, 1.0f, 0.0f));
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, v2ToF2(uvs[i]));
        }
        for (int i = 0; i < 3; i++) {
            index->Write(indexBufIndex++, i);
        }
        for (int i = 2; i < 5; i++) {
            index->Write(indexBufIndex++, i == 4 ? 0 : i);
        }
        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices = indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }

    MeshCreateResult CreateCone(const float2 avSize, int alSections,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent) {

		const float fAngleStep = k2Pif/(float)alSections;
		const float fHalfHeight = avSize.y*0.5f;

        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;

        position->Write(vertexBufIdx, float3(0.0f, fHalfHeight, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, 1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

		// Create vertices for base
        details::CreateCircumference(avSize.x, fAngleStep, -fHalfHeight, [&](float3 pos) {
			float3 norm = v3ToF3(normalize(f3Tov3(pos)));
            position->Write(vertexBufIdx, pos);
            normal->Write(vertexBufIdx, norm);
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, float2(0, 0));
        });

		// Base center vertex
        position->Write(vertexBufIdx, float3(0.0f, -fHalfHeight, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, -1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

        indexBufIndex = details::WrapUpperCap(indexBufIndex, index, 0, 1, alSections);
		indexBufIndex = details::WrapLowerCap(indexBufIndex, index, vertexBufIdx - 1, (vertexBufIdx - 1) - alSections, alSections);

        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices = indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }
    MeshCreateResult CreateBox(
        float3 size,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent
    ) {
        ASSERT(index);
        ASSERT(position);
        ASSERT(color);
        ASSERT(normal);
        ASSERT(uv);

        const float3 halfSize = size * 0.5;
        int lVtxIdx = 0;
        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;

        auto getBoxIdx = [](int i, int x, int y, int z) {
            int idx = i;
            if (x + y + z > 0)
                idx = 3 - i;

            return idx;
        };

        auto getBoxTex = [](int i, int x, int y, int z, float3 vAdd[4]) {
            float3 vTex;

            if (std::abs(x)) {
                vTex.x = vAdd[i].z;
                vTex.y = vAdd[i].y;
            } else if (std::abs(y)) {
                vTex.x = vAdd[i].x;
                vTex.y = vAdd[i].z;
            } else if (std::abs(z)) {
                vTex.x = vAdd[i].x;
                vTex.y = vAdd[i].y;
            }

            // Inverse for negative directions
            if (x + y + z < 0) {
                vTex.x = -vTex.x;
                vTex.y = -vTex.y;
            }
            return vTex;
        };

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    if (x == 0 && y == 0 && z == 0)
                        continue;
                    if (std::abs(x) + std::abs(y) + std::abs(z) > 1)
                        continue;

                    // Direction (could say inverse normal) of the quad.
                    float3 vDir(0);
                    float3 vSide(0);

                    float3 vAdd[4];
                    if (std::abs(x)) {
                        vDir.x = (float)x;

                        vAdd[0] = float3(0, 1, 1);
                        vAdd[1] = float3(0, -1, 1);
                        vAdd[2] = float3(0, -1, -1);
                        vAdd[3] = float3(0, 1, -1);
                    } else if (std::abs(y)) {
                        vDir.y = (float)y;

                        vAdd[0] = float3(1, 0, 1);
                        vAdd[1] = float3(1, 0, 1);
                        vAdd[2] = float3(-1, 0, -1);
                        vAdd[3] = float3(-1, 0, -1);
                    } else if (std::abs(z)) {
                        vDir.z = (float)z;

                        vAdd[0] = float3(1, 1, 0);
                        vAdd[1] = float3(-1, 1, 0);
                        vAdd[2] = float3(-1, -1, 0);
                        vAdd[3] = float3(1, -1, 0);
                    }

                    // Log("Side: (%.0f : %.0f : %.0f) [ ", vDir.x,  vDir.y,vDir.z);
                    for (int i = 0; i < 4; i++) {
                        int idx = getBoxIdx(i, x, y, z);
                        float3 vTex = getBoxTex(i, x, y, z, vAdd);
                        float2 vCoord = float2((vTex.x + 1) * 0.5f, (vTex.y + 1) * 0.5f);

                        color->Write(vertexBufIdx, float4(1, 1, 1, 1));
                        position->Write(vertexBufIdx, (vDir + vAdd[idx]) * size);
                        normal->Write(vertexBufIdx, vDir);
                        uv->Write(vertexBufIdx++, vCoord);
                        vSide = vDir + vAdd[idx];
                        // Log("%d: Tex: (%.1f : %.1f : %.1f) ", i,vTex.x,  vTex.y,vTex.z);
                        // Log("%d: (%.1f : %.1f : %.1f) ", i,vSide.x,  vSide.y,vSide.z);
                    }

                    for (int i = 0; i < 3; i++) {
                        index->Write(indexBufIndex++, lVtxIdx + i);
                    }
                    index->Write(indexBufIndex++, lVtxIdx + 2);
                    index->Write(indexBufIndex++, lVtxIdx + 3);
                    index->Write(indexBufIndex++, lVtxIdx + 0);
                    lVtxIdx += 4;
                }
            }
        }
        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices = indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }

    MeshCreateResult CreateCapsule(
        Vector2 avSize,
        int alSections,
        int alSlices,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent
    ) {
		const float fAngleStep = k2Pif/(float)alSections;
		const float fRadius = avSize.getX();
		const float fHalfHeight = avSize.getY()*0.5f;
		const float fCylinderHalfHeight = cMath::Clamp(fHalfHeight-fRadius, 0,fCylinderHalfHeight);
		const float fInvSlices = 1.0f/alSlices;
		const float fSectionStep = k2Pif/(float)alSections;

        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;

        position->Write(vertexBufIdx, float3(0.0f, cMath::Max(fHalfHeight, fRadius), 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, 1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

		// Slices
		const float fHalfPi		= kPif*0.5f;
		const float fHalfPiOverSlices = fHalfPi*fInvSlices;

		for(int i=0;i<alSlices-1; ++i)
		{
			const float fHeight = fCylinderHalfHeight + fRadius - fRadius*(1+sin(fHalfPiOverSlices*(float)(i+1)-fHalfPi));
			const float fAngle = asin((fHeight-fCylinderHalfHeight)/fRadius);
		    details::CreateCircumference(avSize.getX()*cos(fAngle), fSectionStep, fHeight, [&](float3 pos) {
			    float3 norm = v3ToF3(normalize(f3Tov3(pos) - Vector3(0, fCylinderHalfHeight, 0)));
                position->Write(vertexBufIdx, pos);
                normal->Write(vertexBufIdx, norm);
                color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
                uv->Write(vertexBufIdx++, float2(0, 0));
		    });
		}

		details::CreateCircumference(avSize.getX(), fSectionStep, 0, [&](float3 pos) {
			pos.y = fCylinderHalfHeight;
		    float3 norm = v3ToF3(normalize(f3Tov3(pos)));
            position->Write(vertexBufIdx, pos);
            normal->Write(vertexBufIdx, norm);
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, float2(0, 0));
		});

		details::CreateCircumference(avSize.getX(), fSectionStep, 0, [&](float3 pos) {
			pos.y = -fCylinderHalfHeight;
		    float3 norm = v3ToF3(normalize(f3Tov3(pos)));
            position->Write(vertexBufIdx, pos);
            normal->Write(vertexBufIdx, norm);
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, float2(0, 0));
		});

		for(int i=alSlices-2;i>=0; --i)
		{
			// Same absolute height and radius than upper dome, only reverse order
			const float fHeight = fCylinderHalfHeight + fRadius - fRadius*(1+sin(fHalfPiOverSlices*(float)(i+1)-fHalfPi));
			const float fAngle = asin((fHeight-fCylinderHalfHeight)/fRadius);
			tVector3fVec vVertices;

			// and height is negative here
		    details::CreateCircumference(avSize.getX()*cos(fAngle), fSectionStep, -fHeight, [&](float3 pos) {
			    float3 norm = v3ToF3(normalize(f3Tov3(pos) + Vector3(0, fCylinderHalfHeight, 0)));
                position->Write(vertexBufIdx, pos);
                normal->Write(vertexBufIdx, norm);
                color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
                uv->Write(vertexBufIdx++, float2(0, 0));
		    });
		}
        position->Write(vertexBufIdx, float3(0.0f, cMath::Min(-fHalfHeight, -fRadius), 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, -1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

        indexBufIndex = details::WrapUpperCap(indexBufIndex, index, 0, 1, alSections);
		for(int i=0;i<2*alSlices-1;++i)
		{
			indexBufIndex = details::WrapSides(indexBufIndex, index, 1 + i*alSections, alSections);
		}
		indexBufIndex = details::WrapLowerCap(indexBufIndex, index, vertexBufIdx-1, (vertexBufIdx-1) - alSections, alSections);
        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices = indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }

    MeshCreateResult CreateCylinder(
        Vector2 size,
        uint32_t sections,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float4>* color,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<GraphicsBuffer::BufferStructuredView<float3>*, GraphicsBuffer::BufferStructuredView<float4>*> tangent
    ) {

		const float fAngleStep = k2Pif/(float)sections;
		const float fHalfHeight = size.getY()*0.5f;

        uint32_t vertexBufIdx = 0;
        uint32_t indexBufIndex = 0;

        position->Write(vertexBufIdx, float3(0.0f, fHalfHeight, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, 1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

        details::CreateCircumference(size.getX(), fAngleStep, 0, [&](float3 pos) {
            pos.y = fHalfHeight;
			float3 norm = v3ToF3(normalize(f3Tov3(pos)));
            position->Write(vertexBufIdx, pos);
            normal->Write(vertexBufIdx, norm);
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, float2(0, 0));
        });

        details::CreateCircumference(size.getX(), fAngleStep, 0, [&](float3 pos) {
            pos.y = -fHalfHeight;
			float3 norm = v3ToF3(normalize(f3Tov3(pos)));
            position->Write(vertexBufIdx, pos);
            normal->Write(vertexBufIdx, norm);
            color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
            uv->Write(vertexBufIdx++, float2(0, 0));
        });

        position->Write(vertexBufIdx, float3(0.0f, -fHalfHeight, 0.0f));
        normal->Write(vertexBufIdx, float3(0.0f, -1.0f, 0.0f));
        color->Write(vertexBufIdx, float4(1.0f, 1.0f, 1.0f, 1.0f));
        uv->Write(vertexBufIdx++, float2(0, 0));

		indexBufIndex = details::WrapUpperCap(indexBufIndex, index, 0, 1, sections);
		indexBufIndex = details::WrapSides(indexBufIndex, index, 1, sections);
		indexBufIndex = details::WrapLowerCap(indexBufIndex, index, (vertexBufIdx  - 1), (vertexBufIdx  - 1) - sections, sections);

        CreateTriTangentVectorsHPL2(vertexBufIdx, indexBufIndex, index, position, uv, normal, tangent);
        MeshCreateResult result;
        result.numIndices= indexBufIndex;
        result.numVertices = vertexBufIdx;
        return result;
    }

    void MikkTSpaceGenerate(
        uint32_t numVerts,
        uint32_t numIndecies,
        GraphicsBuffer::BufferIndexView* view,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float3>* writeTangent) {
        details::MikktSpaceUser user;
        user.m_numElements = numVerts;
        user.m_numIndices = numIndecies;
        user.m_indexView = view;
        user.m_position = position;
        user.m_uv = uv;
        user.m_normal = normal;
        user.m_tangent = writeTangent;
        SMikkTSpaceContext context = { 0 };
        SMikkTSpaceInterface handlers = { 0 };
        handlers.m_getNumFaces = details::mikkT_getNumberFaces;
        handlers.m_getNumVerticesOfFace = details::mikkT_getNumVerticesOfFaces;
        handlers.m_getPosition = details::mikkT_getPosition;
        handlers.m_getNormal = details::mikkT_getNormal;
        handlers.m_getTexCoord = details::mikkT_getTexCoord;
        handlers.m_setTSpaceBasic = details::mikkT_setTSpaceBasic;
        context.m_pInterface = &handlers;
        context.m_pUserData = &user;
        bool isSuccess = genTangSpaceDefault(&context);
        ASSERT(isSuccess == true);
    }
} // namespace hpl::MeshUtility
