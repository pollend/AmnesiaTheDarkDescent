#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/sse/vectormath.hpp"
#include <math/Frustum.h>

namespace hpl::math {

    Frustum Frustum::CreateProjection() {
        return Frustum();
    }
    Frustum Frustum::CreateOrthographic() {
        return Frustum();
    }
    Frustum::Frustum():
        m_projection(Matrix4::identity()),
        m_viewProjection(Matrix4::identity()),
        m_view(Matrix4::identity()) {

    }

    Vector3 Frustum::GetForward() {
        return Vector3(0,0,0);
    }
}
