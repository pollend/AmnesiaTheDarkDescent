#pragma once

#include "HPLMemberLayout.h"

namespace hpl {

typedef struct {
  cMatrixf worldTransform;

} TransformUniform;

class HPLTransformLayout : public HPLMemberLayout {};

}