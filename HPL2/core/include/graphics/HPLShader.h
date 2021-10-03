#pragma once

#include "graphics/HPLMemberLayout.h"
#include "graphics/HPLParameter.h"

#include "absl/types/span.h"

#include <Common_3/Renderer/IRenderer.h>
#include <Common_3/Renderer/IResourceLoader.h>

namespace hpl {

enum { GLOBAL_SHADER, MATERIAL_SHADER } HPLShaderType;

class IHPLShader {};

template <class TParameter> class HPLShader : public IHPLShader {
public:

  HPLShader(Renderer *renderer, uint32_t permutation) : _renderer(renderer) {
    uint16_t flags = 0;
    assert(_layout.countByType(HPL_MEMBER_VERTEX_SHADER) <= 1);
    assert(_layout.countByType(HPL_MEMBER_PIXEL_SHADER) <= 1);

    int idx = 0;
    ShaderLoadDesc desc = {};
    std::vector<const char *> _macros[SHADER_STAGE_COUNT];
    hpl::HPLMemberCapture members;
    _layout.byType(HPL_MEMBER_VERTEX_SHADER, members);
    _layout.byType(HPL_MEMBER_PIXEL_SHADER, members);

//    _layout.byType(HPL_MEMBER_VERTEX_SHADER,
//        [&](const char *name, const ShaderMember &member) {
//          auto &macros = _macros[idx];
//          ShaderStageLoadDesc &stage = desc.mStages[idx];
//          stage.pFileName = name;
//          idx++;
//        });
//    _layout.byType<HPL_MEMBER_PIXEL_SHADER>(
//        [&](const char *name, const auto &v) {
//          auto &macros = _macros[idx];
//          ShaderStageLoadDesc &stage = desc.mStages[idx];
//          stage.pFileName = name;
//          idx++;
//        });
  }
  HPLShader(Renderer *renderer, const ShaderLoadDesc &desc)
      : _renderer(renderer) {
    addShader(renderer, &desc, &_shader);
  }
  ~HPLShader() {

  }

private:
  Shader *_shader;
  Renderer *_renderer;
  HPLMemberLayout<TParameter> _layout;
};

class HPLShaderType {};

}

