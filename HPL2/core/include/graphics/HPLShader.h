#pragma once

#include "graphics/HPLParameter.h"
#include <Common_3/Renderer/IRenderer.h>

namespace hpl {
class HPLShader {
public:
  HPLShader(Renderer* renderer, const ShaderMember stages[], size_t numStages, uint32_t features);
  ~HPLShader();
private:
  Shader* _shader;
};

}