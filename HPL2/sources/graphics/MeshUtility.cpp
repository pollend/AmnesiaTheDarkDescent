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
                    position->Set(i, v3ToF3((transform * Vector4(f3Tov3(value), 1.0f)).getXYZ()));
                }
                if(normal) {
                    float3 value = normal->Get(i);
                    normal->Set(i, v3ToF3(normalMat * f3Tov3(value)));
                }
                if(tangent) {
                    float3 value = tangent->Get(i);
                    tangent->Set(i, v3ToF3(normalMat * f3Tov3(value)));
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
	    position->Append(float3(0.0f, max(halfHeight, radius), 0.0f));
        if(normal) {
            normal->Append(float3(0.0f,1.0f,0.0f));
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
                    position->Append(v3ToF3(point));
                    if(normal) {
                        normal->Append(v3ToF3(norm));
                    }
			        if(i == 0) {
                        currentIndexIdx += 3;
                        index->Append(topCenterVtxIdx);
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);
			        } else {
                        currentIndexIdx += 3;
                        index->Append(lastStartEdgeIndex + edgeIndex);
                        index->Append(lastStartEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);

                        currentIndexIdx += 3;
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(lastStartEdgeIndex + edgeIndex - 1);
                        index->Append(lastStartEdgeIndex + edgeIndex);
                    }
                }
                currentVertexIdx += edgeIndex;
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

                position->Append(v3ToF3(p1));
                position->Append(v3ToF3(p2));
                if (normal) {
                    Vector3 norm = Vector3(cos(angle), 0.0f, sin(angle));
                    normal->Append(v3ToF3(norm));
                    normal->Append(v3ToF3(norm));
                }
                if(edgeIndex >= 2) {
                    currentIndexIdx += 3;
                    index->Append(startEdgeIndex + edgeIndex);
                    index->Append(startEdgeIndex + edgeIndex - 2);
                    index->Append(startEdgeIndex + edgeIndex - 1);

                    currentIndexIdx += 3;
                    index->Append(startEdgeIndex + edgeIndex);
                    index->Append(startEdgeIndex + edgeIndex - 1);
                    index->Append(startEdgeIndex + edgeIndex + 1);
                }
            }
            currentVertexIdx += edgeIndex;
            currentIndexIdx += 3;
            index->Append(startEdgeIndex + edgeIndex);
            index->Append(startEdgeIndex);
            index->Append(startEdgeIndex +  1);

            currentIndexIdx += 3;
            index->Append(startEdgeIndex + edgeIndex);
            index->Append(startEdgeIndex + 1);
            index->Append(startEdgeIndex + edgeIndex + 1);
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
                    position->Append(v3ToF3(point));
                    if(normal) {
                        normal->Append(v3ToF3(norm));
                    }
			        if(i == 0) {
                        currentIndexIdx += 3;
                        index->Append(topCenterVtxIdx);
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);
			        } else {
                        currentIndexIdx += 3;
                        index->Append(lastStartEdgeIndex + edgeIndex);
                        index->Append(lastStartEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);

                        currentIndexIdx += 3;
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(lastStartEdgeIndex + edgeIndex - 1);
                        index->Append(lastStartEdgeIndex + edgeIndex);
                    }
                }
                currentVertexIdx += edgeIndex;
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
        position->Append(float3(0.0f, halfHeight, 0.0f));
        if(normal) {
            normal->Append(float3(0,1,0));
        }

        const uint32_t bottomVertexIdx = (currentVertexIdx++);
        position->Append(float3(0.0f, -halfHeight, 0.0f));
        if(normal) {
            normal->Append(float3(0,-1,0));
        }

        uint32_t edgeIndex = 0;
        const uint32_t startEdgeIndex = currentVertexIdx;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep, edgeIndex++) {
			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(-height, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Append(v3ToF3(point));
            Vector3 norm = normalize(point);
            if(normal) {
                normal->Append(v3ToF3(norm));
            }

            if(edgeIndex > 0) {
                currentIndexIdx += 3;
                index->Append(topVertexIdx);
                index->Append(startEdgeIndex + edgeIndex - 1);
                index->Append(startEdgeIndex + edgeIndex);

                currentIndexIdx += 3;
                index->Append(bottomVertexIdx);
                index->Append(startEdgeIndex + edgeIndex - 1);
                index->Append(startEdgeIndex + edgeIndex);

            }
        }
        currentVertexIdx += edgeIndex;
        currentIndexIdx += 3;
        const uint32_t endEdgeIndex = currentVertexIdx - 1;

        index->Append(topVertexIdx);
        index->Append(startEdgeIndex);
        index->Append(endEdgeIndex);

        currentIndexIdx += 3;
        index->Append(bottomVertexIdx);
        index->Append(startEdgeIndex);
        index->Append(endEdgeIndex);
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
        position->Append(float3(0.0f, halfHeight, 0.0f));

        uint32_t boffomCenterVtxIdx = currentVertexIdx++;
        position->Append(float3(0.0f, -halfHeight, 0.0f));

        const uint32_t topStartEdgeIndex = currentVertexIdx;
        uint32_t edgeIndex = 0;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep, edgeIndex++) {

			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(halfHeight, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Append(v3ToF3(point));
            Vector3 norm = normalize(point);
            if(normal) {
                normal->Append(v3ToF3(norm));
            }

            if(edgeIndex > 0) {
                currentIndexIdx += 3;
                index->Append(topCenterVtxIdx);
                index->Append(topStartEdgeIndex  + edgeIndex - 1);
                index->Append(topStartEdgeIndex  + edgeIndex);
            }
        }
        currentVertexIdx += edgeIndex;
        const uint32_t topEndEdgeIndex = currentVertexIdx - 1;
        currentIndexIdx += 3;
        index->Append(topCenterVtxIdx);
        index->Append(edgeIndex - 1);
        index->Append(topStartEdgeIndex + 1);

        const uint32_t bottomStartEdgeIndex = currentVertexIdx;
        edgeIndex = 0;
        for(float angle = 0; angle <= (2.0f * PI - angleStep); angle += angleStep) {

			Vector3 point = Vector3(cMath::RoundFloatToDecimals(radius*cos(angle), 6),
										 cMath::RoundFloatToDecimals(-halfHeight, 6),
										 cMath::RoundFloatToDecimals(radius*sin(angle),6));
            position->Append(v3ToF3(point));
            Vector3 norm = normalize(point);
            if(normal) {
                normal->Append(v3ToF3(norm));
            }
            if(edgeIndex > 0) {
                currentIndexIdx += 3;
                index->Append(boffomCenterVtxIdx);
                index->Append(bottomStartEdgeIndex  + edgeIndex - 1);
                index->Append(bottomStartEdgeIndex  + edgeIndex);

                currentIndexIdx += 3;
                index->Append(topStartEdgeIndex + edgeIndex);
                index->Append(topStartEdgeIndex + edgeIndex - 1);
                index->Append(bottomStartEdgeIndex  + edgeIndex);

                currentIndexIdx += 3;
                index->Append(topStartEdgeIndex + edgeIndex - 1);
                index->Append(bottomStartEdgeIndex + edgeIndex - 1);
                index->Append(bottomStartEdgeIndex + edgeIndex);
            }
        }
        currentVertexIdx += edgeIndex;
        currentIndexIdx += 3;
        const uint32_t bottomEndEdgeIndex = currentVertexIdx - 1;
        index->Append(boffomCenterVtxIdx);
        index->Append(edgeIndex - 1);
        index->Append(bottomStartEdgeIndex + 1);

        currentIndexIdx += 3;
        index->Append(topEndEdgeIndex);
        index->Append(bottomEndEdgeIndex);
        index->Append(bottomStartEdgeIndex);

        currentIndexIdx += 3;
        index->Append(topStartEdgeIndex);
        index->Append(topEndEdgeIndex);
        index->Append(bottomStartEdgeIndex);

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

        if(CreatePlane(p1, p2, indexOffset, vertexOffset, position, normal, index)) {
            if(uvs) {
                uvs->Append(v2ToF2(uvPoints[0]));
                uvs->Append(v2ToF2(uvPoints[1]));
                uvs->Append(v2ToF2(uvPoints[2]));
                uvs->Append(v2ToF2(uvPoints[3]));
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
            for (auto& scaler : dirPoints) {
                position->Append(v3ToF3(mulPerElem(boxSize, scaler)));
                if (normal) {
                    normal->Append(v3ToF3(norm));
                }
            }
            if (uv0) {
                for (auto& coord : texCoords) {
                    uv0->Append(v2ToF2(coord));
                }
            }
            currentVertexIdx += 4;
            currentIndexIdx += 3;
            index->Append(firstIndex);
            index->Append(firstIndex + 1);
            index->Append(firstIndex + 2);

            currentIndexIdx += 3;
            index->Append(firstIndex + 2);
            index->Append(firstIndex + 3);
            index->Append(firstIndex + 0);
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
            position->Append(v3ToF3(p));
            if(normal) {
                normal->Append(v3ToF3(n));
            }
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
            index->Append(currentVertexIdx + 0);
            index->Append(currentVertexIdx + 2);
            index->Append(currentVertexIdx + 1);
        } else {
            index->Append(currentVertexIdx + 0);
            index->Append(currentVertexIdx + 1);
            index->Append(currentVertexIdx + 2);
        }

        if(winding == PlaneWinding::CCW) {
            index->Append(currentVertexIdx + 0);
            index->Append(currentVertexIdx + 3);
            index->Append(currentVertexIdx + 2);
        } else {
            index->Append(currentVertexIdx + 0);
            index->Append(currentVertexIdx + 2);
            index->Append(currentVertexIdx + 3);
        }
        currentVertexIdx += 4;
        currentIndexIdx += 6;

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
        position->Append(float3(0.0f,radius,0.0f));
        normal->Append(float3(0.0f,1.0f, 0.0f));

        const uint32_t bottomCenterVtxIdx = currentVertexIdx++;
        position->Append(float3(0.0f,-radius,0.0f));
		if(normal) {
            normal->Append(float3(0.0f,-1.0f, 0.0f));
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
			        position->Append(v3ToF3(point));
			        if(normal) {
			            normal->Append(v3ToF3(norm));
			        }
			        if(i == 0) {
                        currentIndexIdx += 3;
                        index->Append(topCenterVtxIdx);
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);
			        } else {
                        currentIndexIdx += 3;
                        index->Append(lastGroupIdx + edgeIndex);
                        index->Append(lastGroupIdx + edgeIndex - 1);
                        index->Append(startEdgeIndex + edgeIndex);

                        currentIndexIdx += 3;
                        index->Append(startEdgeIndex + edgeIndex - 1);
                        index->Append(startEdgeIndex  + edgeIndex);
                        index->Append(lastGroupIdx + edgeIndex);
                        if(i == horizontalSlices - 1) {
                            currentIndexIdx += 3;
                            index->Append(bottomCenterVtxIdx);
                            index->Append(startEdgeIndex + edgeIndex - 1);
                            index->Append(startEdgeIndex  + edgeIndex);
                        }
			        }
                }
                currentVertexIdx += edgeIndex;
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



