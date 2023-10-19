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
}
