#pragma once
#include "graphics/HPLShader.h"
#include <unordered_map>

template <class TShader>
class HPLShaderType<TShader> {
public:
  explicit HPLShaderType(Renderer* renderer): _renderer(renderer) {}
  ~HPLShaderType() = default;

  void configureDefault(TShader* shader) = 0;
  std::unique_ptr<TShader> get(uint32_t permutation) {
    auto shader = _shaders.find(permutation);
    if(shader != _shaders.end()) {
      std::make_unique<>(shader->second);
    } else {
      auto* temp = new typename TShader(_renderer, permutation);
      _shaders[permutation] = temp;
    }
  }
private:
  std::unorder_map<uint32_t, TShader*> _shaders;
  Renderer* _renderer;
};