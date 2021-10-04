#pragma once

#include "graphics/HPLMemberLayout.h"
#include "graphics/HPLParameter.h"

#include "absl/types/span.h"

#include <Common_3/Renderer/IRenderer.h>
#include <Common_3/Renderer/IResourceLoader.h>

namespace hpl {

enum { HPL_GLOBAL_SHADER, HPL_MATERIAL_SHADER } HPLShaderType;

class IHPLShader {
  virtual Shader *HALShader() = 0;
  virtual RootSignature *HALRootSignature() = 0;
};

template <class TMemberLayout> class HPLShader : public IHPLShader {
public:
  HPLShader(HPLShader& copy): _shader(copy.HALShader()) {

  }

  HPLShader(Renderer *renderer, uint32_t permutation) : _renderer(renderer) {
    assert(_layout.countByType(HPL_MEMBER_VERTEX_SHADER) <= 1);
    assert(_layout.countByType(HPL_MEMBER_PIXEL_SHADER) <= 1);

    ShaderLoadDesc desc = {};
    absl::InlinedVector<ShaderMacro, 10> _macros[SHADER_STAGE_COUNT];
    hpl::HPLMemberCapture members;
    _layout.byType(HPL_MEMBER_VERTEX_SHADER, members);
    _layout.byType(HPL_MEMBER_PIXEL_SHADER, members);

    int stageCount = 0;
    for (auto &mem : members) {
      size_t current_index = stageCount++;
      auto &stage = desc.mStages[current_index];
      stage.pFileName = mem->memberName;
      for (const auto &perm : mem->member_shader.permutations) {
        if ((perm.bits & permutation) > 0) {
          _macros[current_index].push_back(
              {.definition = perm.key, .value = perm.value});
        }
      }
      stage.pMacros = _macros[current_index].data();
      stage.mMacroCount = _macros[current_index].size();
    }
    addShader(renderer, &desc, &_shader);
    RootSignatureDesc rootDesc = {
        .ppShaders = &_shader,
        .mShaderCount = 1,
    };
    addRootSignature(renderer, &rootDesc, &_signature);
  }
  HPLShader(Renderer *renderer, const ShaderLoadDesc &desc)
      : _renderer(renderer) {
    addShader(renderer, &desc, &_shader);
  }
  ~HPLShader() {}

  Shader *HALShader() override { return _shader; }
  RootSignature *HALRootSignature() override { return _signature; }
//  typename TMemberLayout::TParamType &parameter() { return _parameter; }

protected:
private:
  Shader *_shader;
  RootSignature *_signature;
  Renderer *_renderer;
  TMemberLayout _layout;
  typename TMemberLayout::TParamType _parameter;
};

}

