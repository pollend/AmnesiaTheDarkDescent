#pragma once

#include "HPLMemberLayout.h"
#include "graphics/HPLParameter.h"
#include <Common_3/Renderer/IRenderer.h>

#include "absl/types/span.h"

namespace hpl {

enum {
  GLOBAL_SHADER,
  MATERIAL_SHADER
} HPLShaderType ;

class HPLShader {
public:

  HPLShader(Renderer* renderer, IHPLMemberLayout* members, const std::vector<ShaderMember>& shader) {
    members->byType<HPL_MEMBER_PIXEL_SHADER>([](const auto &shader) {});
    members->byType<HPL_MEMBER_STRUCT>([](const auto &member) {
    });
  }
  ~HPLShader();

  const absl::Span<HPLMember>& members();
private:
  Shader* _shader;
};

class HPLShaderType {

};

}

