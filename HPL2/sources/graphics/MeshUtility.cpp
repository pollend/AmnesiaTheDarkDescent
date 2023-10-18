#include "graphics/MeshUtility.h"
#include "math/Math.h"


#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

#include <cfloat>
#include <cstdint>


namespace hpl::MeshUtility {

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
                    position->Insert(i, v3ToF3((transform * Vector4(f3Tov3(value), 1.0f)).getXYZ()));
                }
                if(normal) {
                    float3 value = normal->Get(i);
                    normal->Insert(i, v3ToF3(normalMat * f3Tov3(value)));
                }
                if(tangent) {
                    float3 value = tangent->Get(i);
                    tangent->Insert(i, v3ToF3(normalMat * f3Tov3(value)));
                }

            }

    }
    void CreateCapsule(
        float radius,
        float height,
        uint32_t sections,
        uint32_t slices,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index) {

        ASSERT(position);
        ASSERT(index);

        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

	    const float halfHeight = height*0.5f;
		const float cylinderHalfHeight = max(0.0f, halfHeight-radius);
		const float invSlices = 1.0f/slices;
		const float sectionStep = k2Pif/static_cast<float>(sections);
		const float halfPiOverSlices = (PI / 2.0f)* invSlices;

        const uint32_t topCenterVtxIdx = currentVertexIdx++;
	    position->Insert(topCenterVtxIdx, float3(0.0f, max(halfHeight, radius), 0.0f));
        if(normal) {
            normal->Insert(topCenterVtxIdx,float3(0.0f,1.0f,0.0f));
        }
        {
            const uint32_t startEdgeIndex = currentVertexIdx;
            uint32_t lastStartEdgeIndex = 0;
            for(uint32_t i = 0; i < slices - 1; i++) {
                const float height = cylinderHalfHeight + radius - radius * (1.0f + sin(halfPiOverSlices * static_cast<float>(i + 1) - (PI / 2.0f)));
                const float sliceRadius = radius * cos(asin(height / radius));
                uint32_t edgeIndex = 0;
                for (float angle = 0; angle <= (2.0f * PI - sectionStep); angle += sectionStep, edgeIndex++) {
                    const Vector3 point = Vector3(cMath::RoundFloatToDecimals(sliceRadius*cos(angle), 6),
                                                cMath::RoundFloatToDecimals(height, 6),
                                                cMath::RoundFloatToDecimals(sliceRadius*sin(angle),6));
                    Vector3 norm = normalize(point - Vector3(0, cylinderHalfHeight, 0));
                    position->Insert(currentVertexIdx,v3ToF3(point));
                    if(normal) {
                        normal->Insert(currentVertexIdx, v3ToF3(norm));
                    }
                    currentVertexIdx++;
                    if(i == 0) {
                        index->Insert(currentIndexIdx++, topCenterVtxIdx);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
			        } else {
                        index->Insert(currentIndexIdx++, lastStartEdgeIndex + edgeIndex);
                        index->Insert(currentIndexIdx++, lastStartEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);

                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, lastStartEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, lastStartEdgeIndex + edgeIndex);
                    }
                }
                lastStartEdgeIndex = startEdgeIndex;
            }
        }

        {
            uint32_t edgeIndex = 0;
            const uint32_t startEdgeIndex = currentVertexIdx;
            for (float angle = 0; angle <= (2.0f * PI - sectionStep); angle += sectionStep, edgeIndex += 2) {
                Vector3 p1 = Vector3(
                    cMath::RoundFloatToDecimals(radius * cos(angle), 6),
                    cMath::RoundFloatToDecimals(cylinderHalfHeight, 6),
                    cMath::RoundFloatToDecimals(radius * sin(angle), 6));

                Vector3 p2 = Vector3(
                    cMath::RoundFloatToDecimals(radius * cos(angle), 6),
                    cMath::RoundFloatToDecimals(-cylinderHalfHeight, 6),
                    cMath::RoundFloatToDecimals(radius * sin(angle), 6));

                position->Insert(currentVertexIdx++,v3ToF3(p1));
                position->Insert(currentVertexIdx++,v3ToF3(p2));
                if (normal) {
                    Vector3 norm = Vector3(cos(angle), 0.0f, sin(angle));
                    normal->Insert(currentVertexIdx - 2, v3ToF3(norm));
                    normal->Insert(currentVertexIdx - 1, v3ToF3(norm));
                } if(edgeIndex >= 2) {
                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 2);
                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);

                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                    index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex + 1);
                }
            }
            index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
            index->Insert(currentIndexIdx++, startEdgeIndex);
            index->Insert(currentIndexIdx++, startEdgeIndex +  1);

            index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
            index->Insert(currentIndexIdx++, startEdgeIndex + 1);
            index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex + 1);
        }
        {
            const uint32_t startEdgeIndex = currentVertexIdx;
            uint32_t lastStartEdgeIndex = 0;
            for(uint32_t i = 0; i < slices - 1; i++) {
                const float height = cylinderHalfHeight + radius - radius * (1.0f + sin(halfPiOverSlices * static_cast<float>(i + 1) - (PI / 2.0f)));
                const float sliceRadius = radius * cos(asin(height / radius));
                uint32_t edgeIndex = 0;
                for (float angle = 0; angle <= (2.0f * PI - sectionStep); angle += sectionStep, edgeIndex++) {
		            const Vector3 point = Vector3(cMath::RoundFloatToDecimals(sliceRadius*cos(angle), 6),
										         cMath::RoundFloatToDecimals(-height, 6),
										         cMath::RoundFloatToDecimals(sliceRadius*sin(angle),6));
			        Vector3 norm = normalize(point + Vector3(0, cylinderHalfHeight, 0));
                    position->Insert(currentVertexIdx, v3ToF3(point));
                    if(normal) {
                        normal->Insert(currentVertexIdx,v3ToF3(norm));
                    }
                    currentVertexIdx++;
			        if(i == 0) {
                        index->Insert(currentIndexIdx++,topCenterVtxIdx);
                        index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex);
			        } else {
                        index->Insert(currentIndexIdx++,lastStartEdgeIndex + edgeIndex);
                        index->Insert(currentIndexIdx++,lastStartEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex);

                        index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++,lastStartEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++,lastStartEdgeIndex + edgeIndex);
                    }
                }
                lastStartEdgeIndex = startEdgeIndex;
            }
        }
        if(indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if(vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }
    }

    void CreateCone(
        float radius,
        float height,
        uint32_t numberOfSections,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index
    ) {
        ASSERT(position);
        ASSERT(index);
		float angleStep = (2.0f * PI)/static_cast<float>(numberOfSections);
		float halfHeight = height * 0.5f;

        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

        const uint32_t topVertexIdx = (currentVertexIdx++);
        position->Insert(topVertexIdx,float3(0.0f, halfHeight, 0.0f));
        if(normal) {
            normal->Insert(topVertexIdx,float3(0,1,0));
        }

        const uint32_t bottomVertexIdx = (currentVertexIdx++);
        position->Insert(bottomVertexIdx,float3(0.0f, -halfHeight, 0.0f));
        if(normal) {
            normal->Insert(bottomVertexIdx,float3(0,-1,0));
        }

        uint32_t edgeIndex = 0;
        const uint32_t startEdgeIndex = currentVertexIdx;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep, edgeIndex++) {
			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(-height, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Insert(currentVertexIdx, v3ToF3(point));
            if(normal) {
                Vector3 norm = normalize(point);
                normal->Insert(currentVertexIdx,v3ToF3(norm));
            }
            currentVertexIdx++;

            if(edgeIndex > 0) {
                index->Insert(currentIndexIdx++,topVertexIdx);
                index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex - 1);
                index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex);

                index->Insert(currentIndexIdx++,bottomVertexIdx);
                index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex - 1);
                index->Insert(currentIndexIdx++,startEdgeIndex + edgeIndex);

            }
        }
        const uint32_t endEdgeIndex = currentVertexIdx - 1;

        index->Insert(currentIndexIdx++, topVertexIdx);
        index->Insert(currentIndexIdx++,startEdgeIndex);
        index->Insert(currentIndexIdx++,endEdgeIndex);

        index->Insert(currentIndexIdx++,bottomVertexIdx);
        index->Insert(currentIndexIdx++,startEdgeIndex);
        index->Insert(currentIndexIdx++,endEdgeIndex);
        if(indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if(vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }

    }

    bool CreateCylinder(
        float radius,
        float height,
        uint32_t sections,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index) {

        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

        const float angleStep = (2 * PI) / static_cast<float>(sections);
        const float halfHeight = height * 0.5f;

        uint32_t topCenterVtxIdx = currentVertexIdx++;
        position->Insert(topCenterVtxIdx, float3(0.0f, halfHeight, 0.0f));

        uint32_t boffomCenterVtxIdx = currentVertexIdx++;
        position->Insert(boffomCenterVtxIdx, float3(0.0f, -halfHeight, 0.0f));

        const uint32_t topStartEdgeIndex = currentVertexIdx;
        uint32_t edgeIndex = 0;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep, edgeIndex++) {

			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(halfHeight, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Insert(currentVertexIdx,v3ToF3(point));
            if(normal) {
                Vector3 norm = normalize(point);
                normal->Insert(currentVertexIdx,v3ToF3(norm));
            }
            currentVertexIdx++;

            if(edgeIndex > 0) {
                index->Insert(currentIndexIdx++, topCenterVtxIdx);
                index->Insert(currentIndexIdx++, topStartEdgeIndex  + edgeIndex - 1);
                index->Insert(currentIndexIdx++, topStartEdgeIndex  + edgeIndex);
            }
        }
        const uint32_t topEndEdgeIndex = currentVertexIdx - 1;
        index->Insert(currentIndexIdx++,topCenterVtxIdx);
        index->Insert(currentIndexIdx++,edgeIndex - 1);
        index->Insert(currentIndexIdx++,topStartEdgeIndex + 1);

        const uint32_t bottomStartEdgeIndex = currentVertexIdx;
        edgeIndex = 0;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep) {

			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(-halfHeight, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Insert(currentVertexIdx, v3ToF3(point));
            if(normal) {
                Vector3 norm = normalize(point);
                normal->Insert(currentVertexIdx,v3ToF3(norm));
            }
            currentVertexIdx++;
            if(edgeIndex > 0) {
                index->Insert(currentIndexIdx++, boffomCenterVtxIdx);
                index->Insert(currentIndexIdx++, bottomStartEdgeIndex  + edgeIndex - 1);
                index->Insert(currentIndexIdx++, bottomStartEdgeIndex  + edgeIndex);

                index->Insert(currentIndexIdx++, topStartEdgeIndex + edgeIndex);
                index->Insert(currentIndexIdx++, topStartEdgeIndex + edgeIndex - 1);
                index->Insert(currentIndexIdx++, bottomStartEdgeIndex  + edgeIndex);

                index->Insert(currentIndexIdx++, topStartEdgeIndex + edgeIndex - 1);
                index->Insert(currentIndexIdx++, bottomStartEdgeIndex + edgeIndex - 1);
                index->Insert(currentIndexIdx++, bottomStartEdgeIndex + edgeIndex);
            }
        }
        const uint32_t bottomEndEdgeIndex = currentVertexIdx - 1;
        index->Insert(currentIndexIdx++, boffomCenterVtxIdx);
        index->Insert(currentIndexIdx++, edgeIndex - 1);
        index->Insert(currentIndexIdx++, bottomStartEdgeIndex + 1);

        index->Insert(currentIndexIdx++, topEndEdgeIndex);
        index->Insert(currentIndexIdx++, bottomEndEdgeIndex);
        index->Insert(currentIndexIdx++, bottomStartEdgeIndex);

        index->Insert(currentIndexIdx++, topStartEdgeIndex);
        index->Insert(currentIndexIdx++, topEndEdgeIndex);
        index->Insert(currentIndexIdx++, bottomStartEdgeIndex);

        if(indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if(vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }
        return true;
    }


    bool CreatePlane(
        const Vector3& p1,
        const Vector3& p2,
        const std::array<Vector2, 4> uvPoints,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float2>* uvs,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index) {
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;
        if(CreatePlane(p1, p2, indexOffset, vertexOffset, position, normal, index)) {
            if(uvs) {
                uvs->Insert(currentVertexIdx++, v2ToF2(uvPoints[0]));
                uvs->Insert(currentVertexIdx++, v2ToF2(uvPoints[1]));
                uvs->Insert(currentVertexIdx++, v2ToF2(uvPoints[2]));
                uvs->Insert(currentVertexIdx++, v2ToF2(uvPoints[3]));
            }
            return true;
        }
        return false;
    }

    bool CreateBox(
        const Vector3& boxSize,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv0,
        AssetBuffer::BufferIndexView* index) {
        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

        std::array<float, 2> direction = { -1, 1 };
        auto addFace = [&](const std::array<Vector2, 4> texCoords, const std::array<Vector3, 4>& dirPoints, const Vector3& norm) {
            const uint32_t firstIndex = currentVertexIdx;
            for(size_t i = 0; i < 4; i++) {
                position->Insert(currentVertexIdx, v3ToF3(mulPerElem(boxSize, dirPoints[i])));
                if (normal) {
                    normal->Insert(currentVertexIdx, v3ToF3(norm));
                }
                if(uv0) {
                    uv0->Insert(currentVertexIdx, v2ToF2(texCoords[i]));
                }
                currentVertexIdx++;
            }
            index->Insert(currentIndexIdx++,firstIndex);
            index->Insert(currentIndexIdx++,firstIndex + 1);
            index->Insert(currentIndexIdx++,firstIndex + 2);

            index->Insert(currentIndexIdx++,firstIndex + 2);
            index->Insert(currentIndexIdx++,firstIndex + 3);
            index->Insert(currentIndexIdx++,firstIndex + 0);
        };

        for (auto& dir : direction) {
            // x-directions
            std::array<Vector2, 4> texCoords = { ((Vector2(1.0f, 1.0f) * dir) + Vector2(1, 1)) / 2.0f,
                                                 ((Vector2(-1.0f, 1.0f) * dir) + Vector2(1, 1)) / 2.0f,
                                                 ((Vector2(-1.0f, -1.0f) * dir) + Vector2(1, 1)) / 2.0f,
                                                 ((Vector2(1.0f, -1.0f) * dir) + Vector2(1, 1)) / 2.0f };
            {
                std::array<Vector3, 4> pointDir = {
                    Vector3(1.0f, 1.0f, dir), Vector3(-1.0f, 1.0f, dir), Vector3(-1.0f, -1.0f, dir), Vector3(1.0f, -1.0f, dir)
                };
                addFace(texCoords, pointDir, Vector3(0, 0, dir));
            }
            // y-direction
            {
                std::array<Vector3, 4> pointDir = {
                    Vector3(1.0f, dir, 1.0f), Vector3(-1.0f, dir, 1.0f), Vector3(-1.0f, dir, -1.0f), Vector3(1.0f, dir, -1.0f)
                };
                addFace(texCoords, pointDir, Vector3(0, dir, 0));
            }
            // z-direction
            {
                std::array<Vector3, 4> pointDir = {
                    Vector3(dir, 1.0f, 1.0f), Vector3(dir, -1.0f, 1.0f), Vector3(dir, -1.0f, -1.0f), Vector3(dir, 1.0f, -1.0f)
                };
                addFace(texCoords, pointDir, Vector3(dir, 0, 0));
            }
        }
        if (indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if (vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }

        return true;
    }

    bool CreatePlane(
        const Vector3& p1,
        const Vector3& p2,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index) {
        ASSERT(position);
        ASSERT(index);
        if(p1 == p2) {
            return false;
        }

        const Vector3 corner1 = Vector3(
            min(p1.getX(), p2.getX()),
            min(p1.getY(), p2.getY()),
            min(p1.getZ(), p2.getZ())
        );
        const Vector3 corner2 = Vector3(
            max(p1.getX(), p2.getX()),
            max(p1.getY(), p2.getY()),
            max(p1.getZ(), p2.getZ())
        );

        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

		Vector3 diff = p1 - p2;
        enum PlaneAxis: uint8_t {
            None,
            XAxis,
            YAxis,
            ZAxis
        };
        enum PlaneWinding: uint8_t {
            CW,
            CCW
        };
        PlaneAxis axis = PlaneAxis::None;
        PlaneWinding winding = PlaneWinding::CCW;
        uint32_t numMatchingAxises = 0;
        if(abs(diff.getX()) <= FLT_EPSILON) {
            if(p1.getY() < p2.getY() && p1.getZ() > p2.getZ()) {
                winding = PlaneWinding::CW;
            }
            axis = PlaneAxis::XAxis;
            numMatchingAxises ++;
        }
        if(abs(diff.getY()) <= FLT_EPSILON) {
            if(p1.getX() < p2.getX() && p1.getZ() > p2.getZ()) {
                winding = PlaneWinding::CW;
            }
            axis = PlaneAxis::YAxis;
            numMatchingAxises ++;
        }
        if(abs(diff.getZ()) <= FLT_EPSILON) {
            if(p1.getX() < p2.getX() && p1.getY() > p2.getY()) {
                winding = PlaneWinding::CW;
            }
            axis = PlaneAxis::ZAxis;
            numMatchingAxises ++;
        }
        if(numMatchingAxises != 1 || axis == PlaneAxis::None) {
            return false;
        }

        auto pushVertex = [&](const Vector3& p,const Vector3& n) {
            position->Insert(currentVertexIdx, v3ToF3(p));
            if(normal) {
                normal->Insert(currentVertexIdx,v3ToF3(n));
            }
            currentVertexIdx++;
        };

        switch(axis) {
            case PlaneAxis::XAxis: {
                pushVertex(corner1, Vector3(1, 0, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner1.getX(), corner2.getY(), corner1.getZ()), Vector3(1, 0, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(corner2, Vector3(1, 0, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner1.getX(), corner1.getY(), corner2.getZ()), Vector3(1, 0, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                break;
            }
            case PlaneAxis::YAxis: {
                pushVertex(corner1, Vector3(0, 1, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner2.getX(), corner1.getY(), corner1.getZ()), Vector3(0, 1, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(corner2, Vector3(0, 1, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner1.getX(), corner1.getY(), corner2.getZ()), Vector3(0, 1, 0) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                break;
            }
            case PlaneAxis::ZAxis: {
                pushVertex(corner1, Vector3(0, 0, 1) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner2.getX(), corner1.getY(), corner1.getZ()), Vector3(0, 0, 1) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(corner2, Vector3(0, 0, 1) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                pushVertex(Vector3(corner1.getX(), corner2.getY(), corner1.getZ()), Vector3(0, 0, 1) * (winding == PlaneWinding::CCW ? 1.0f : -1.0f));
                break;
            }
            default:
                ASSERT(false);
                break;
        }
        if(winding == PlaneWinding::CCW) {
            index->Insert(currentIndexIdx++,currentVertexIdx + 0);
            index->Insert(currentIndexIdx++,currentVertexIdx + 2);
            index->Insert(currentIndexIdx++,currentVertexIdx + 1);
        } else {
            index->Insert(currentIndexIdx++,currentVertexIdx + 0);
            index->Insert(currentIndexIdx++,currentVertexIdx + 1);
            index->Insert(currentIndexIdx++,currentVertexIdx + 2);
        }

        if(winding == PlaneWinding::CCW) {
            index->Insert(currentIndexIdx++,currentVertexIdx + 0);
            index->Insert(currentIndexIdx++,currentVertexIdx + 3);
            index->Insert(currentIndexIdx++,currentVertexIdx + 2);
        } else {
            index->Insert(currentIndexIdx++,currentVertexIdx + 0);
            index->Insert(currentIndexIdx++,currentVertexIdx + 2);
            index->Insert(currentIndexIdx++,currentVertexIdx + 3);
        }

        if(indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if(vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }
        return true;
    }

    void CreateUVSphere(
        float radius,
        uint32_t verticalSlices,
        uint32_t horizontalSlices,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index) {
        ASSERT(position);
        ASSERT(index);

        uint32_t currentIndexIdx = indexOffset ? (*indexOffset) : 0;
        uint32_t currentVertexIdx = vertexOffset ? (*vertexOffset) : 0;

        const float invSlices = 1.0f / static_cast<float>(horizontalSlices);
        const float diameter = 2.0f * radius;
        const float heightStep = diameter * invSlices;
        const float sectionStep = (2.0f * PI) / static_cast<float>(verticalSlices);
        const float piOverSlices = PI * invSlices;

        const uint32_t topCenterVtxIdx = currentVertexIdx++;
        position->Insert(topCenterVtxIdx, float3(0.0f,radius,0.0f));
		if(normal) {
            normal->Insert(topCenterVtxIdx,float3(0.0f,1.0f, 0.0f));
        }

        const uint32_t bottomCenterVtxIdx = currentVertexIdx++;
        position->Insert(bottomCenterVtxIdx, float3(0.0f,-radius,0.0f));
		if(normal) {
            normal->Insert(bottomCenterVtxIdx, float3(0.0f,-1.0f, 0.0f));
        }
        {
            uint32_t lastGroupIdx = 0;
            for (uint32_t i = 0; i < horizontalSlices - 1; ++i) {
                const float height = radius - radius * (1 + sin(piOverSlices * (float)(i + 1) - (PI * 0.5f)));
                const float sliceRadius = radius * cos(asin(height / radius));
                uint32_t edgeIndex = 0;
                const uint32_t startEdgeIndex = currentVertexIdx;
                for (float angle = 0; angle <= (2.0f * PI - sectionStep); angle += sectionStep, edgeIndex++) {

			        Vector3 point = Vector3(cMath::RoundFloatToDecimals(sliceRadius*cos(angle), 6),
										         cMath::RoundFloatToDecimals(height, 6),
										         cMath::RoundFloatToDecimals(sliceRadius*sin(angle),6));
			        Vector3 norm = normalize(point);
			        position->Insert(currentVertexIdx, v3ToF3(point));
			        if(normal) {
			            normal->Insert(currentVertexIdx,v3ToF3(norm));
			        }
			        currentVertexIdx++;
			        if(i == 0) {
                        index->Insert(currentIndexIdx++, topCenterVtxIdx);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);
			        } else {
                        index->Insert(currentIndexIdx++, lastGroupIdx + edgeIndex);
                        index->Insert(currentIndexIdx++, lastGroupIdx + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex);

                        index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                        index->Insert(currentIndexIdx++, startEdgeIndex  + edgeIndex);
                        index->Insert(currentIndexIdx++, lastGroupIdx + edgeIndex);
                        if(i == horizontalSlices - 1) {
                            index->Insert(currentIndexIdx++, bottomCenterVtxIdx);
                            index->Insert(currentIndexIdx++, startEdgeIndex + edgeIndex - 1);
                            index->Insert(currentIndexIdx++, startEdgeIndex  + edgeIndex);
                        }
			        }
                }
                lastGroupIdx = startEdgeIndex;
            }
        }

        if(indexOffset) {
            (*indexOffset) = currentIndexIdx;
        }

        if(vertexOffset) {
            (*vertexOffset) = currentVertexIdx;
        }
    }
}



