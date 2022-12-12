#pragma once

#include "absl/types/span.h"
#include "bgfx/bgfx.h"

#include "graphics/Buffer.h"

namespace hpl::vertex::helper {

    void SetAttribute(absl::Span<Buffer*> view, size_t index, bgfx::Attrib::Enum attribute, bool normalized, const float output[4]);
    bool GetAttribute(absl::Span<Buffer*> view, size_t index, bgfx::Attrib::Enum attribute, float output[4]);
    bool HasAttribute(absl::Span<Buffer*> view);

    cBoundingVolume GetBoundedVolume(absl::Span<Buffer*> view, size_t offset, size_t size);
    void UpdateTangentsFromPositionTexCoords(const Buffer* indexBuffer, absl::Span<Buffer*> view, size_t size);
    void Transform(absl::Span<Buffer*> view, const cMatrixf& mtx, size_t offset, size_t size);
}