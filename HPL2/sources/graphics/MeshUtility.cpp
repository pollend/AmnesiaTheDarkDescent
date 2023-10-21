#include "graphics/MeshUtility.h"
#include "Common_3/Utilities/Log/Log.h"
#include "graphics/AssetBuffer.h"
#include "graphics/mikktspace.h"
#include "math/Math.h"

#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

#include <cfloat>
#include <cstdint>

namespace hpl::MeshUtility {
    namespace details {
        struct MikktSpaceUser {
            uint32_t m_numElements;
            uint32_t m_numIndecies;
            AssetBuffer::BufferIndexView* m_indexView;
            AssetBuffer::BufferStructuredView<float3>* m_position;
            AssetBuffer::BufferStructuredView<float2>* m_uv;
            AssetBuffer::BufferStructuredView<float3>* m_normal;
            AssetBuffer::BufferStructuredView<float3>* m_tangent;
        };

        int mikkT_getNumberFaces(const SMikkTSpaceContext * pContext) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            return user->m_numIndecies / 3;
        }

        int mikkT_getNumVerticesOfFaces(const SMikkTSpaceContext * pContext, const int iFace) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            return 3;
        }

        void mikkT_getPosition(const SMikkTSpaceContext * pContext, float fvPosOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float3 v = user->m_position->Get(vertIdx);

            fvPosOut[0] = v.getX();
            fvPosOut[1] = v.getY();
            fvPosOut[2] = v.getZ();
        }
        void mikkT_getNormal(const SMikkTSpaceContext * pContext, float fvNormOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float3 v = user->m_normal->Get(vertIdx);

            fvNormOut[0] = v.getX();
            fvNormOut[1] = v.getY();
            fvNormOut[2] = v.getZ();
        }
        void mikkT_getTexCoord(const SMikkTSpaceContext * pContext, float fvTexcOut[], const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            float2 v = user->m_uv->Get(vertIdx);

            fvTexcOut[0] = v.getX();
            fvTexcOut[1] = v.getY();
        }
        void mikkT_setTSpaceBasic(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
            MikktSpaceUser* user = reinterpret_cast<MikktSpaceUser*>(pContext->m_pUserData);
            uint32_t vertIdx = user->m_indexView->Get((iFace * 3) + iVert);
            user->m_tangent->Write(vertIdx, float3(fvTangent[0], fvTangent[1], fvTangent[2]));
        }
    }

    void Transform(
        uint32_t numElements,
        const Matrix4& transform,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* tangent
    ) {
        Matrix3 normalMat = transpose(inverse(transform.getUpper3x3()));
            for(size_t i = 0; i < numElements; i++) {
                if(position) {
                    float3 value = position->Get(i);
                    position->Write(i, v3ToF3((transform * Vector4(f3Tov3(value), 1.0f)).getXYZ()));
                }
                if(normal) {
                    float3 value = normal->Get(i);
                    normal->Write(i, v3ToF3(normalMat * f3Tov3(value)));
                }
                if(tangent) {
                    float3 value = tangent->Get(i);
                    tangent->Write(i, v3ToF3(normalMat * f3Tov3(value)));
                }

            }

    }

    void MikktSpaceGenerate(
        uint32_t numVerts,
        uint32_t numIndecies,
        AssetBuffer::BufferIndexView* view,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float2>* uv,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* writeTangent
    ) {
        details::MikktSpaceUser user;
        user.m_numElements = numVerts;
        user.m_numIndecies = numIndecies;
        user.m_indexView = view;
        user.m_position = position;
        user.m_uv = uv;
        user.m_normal = normal;
        user.m_tangent = writeTangent;
        SMikkTSpaceContext context = {0};
        SMikkTSpaceInterface handlers = {0};
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
}
