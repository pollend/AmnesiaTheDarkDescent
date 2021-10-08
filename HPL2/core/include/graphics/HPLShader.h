#pragma once

#include "graphics/HPLMemberLayout.h"
#include "graphics/HPLParameter.h"

#include <unordered_map>

#include "absl/types/span.h"

#include <Common_3/Renderer/IRenderer.h>
#include <Common_3/Renderer/IResourceLoader.h>

namespace hpl {

enum { HPL_GLOBAL_SHADER, HPL_MATERIAL_SHADER } HPLShaderType;

class IHPLShader {
public:
  struct HPLShaderType {
    const char *name;
    Shader *shader;
  };

  static void configureShader(Renderer *render, const HPLMember &member,
                              HPLShaderType &shaderTypes, uint32_t permutation);
  static void configureRootSignature(const IHPLMemberLayout &layout,
                                     const absl::Span<Shader *> &shader);

  virtual Shader *getShader() = 0;
};

template <class TMemberLayout> class HPLShader : public IHPLShader {
public:
  using TParamType = typename TMemberLayout::TParamType;

  //  HPLShader(HPLShader& copy): _shader(copy.HALShader()) {
  //
  //  }

  HPLShader(Renderer *renderer, const IHPLMemberLayout &layout,
            uint32_t permutation)
      : _renderer(renderer) {
    hpl::HPLMemberCapture members;
    layout.byType(HPL_MEMBER_SHADER, members);
    for (auto &mem : members) {
      HPLShaderType shaderType = {};
      configureShader(renderer, *mem, shaderType, permutation);
      _shaderTypes.push_back(shaderType);
    }
  }

  ~HPLShader() {}

  TParamType &parameter() { return _parameter; }

protected:
  void addShaderType(const HPLShaderType &shaderType) {
    _shaderTypes.push_back(shaderType);
  }

private:
  Shader *_shader;
  Renderer *_renderer;
  RootSignature *_rootSignature;
  TMemberLayout _layout;
  TParamType _parameter;
  absl::InlinedVector<const HPLShaderType, 4> _shaderTypes;
};

}

