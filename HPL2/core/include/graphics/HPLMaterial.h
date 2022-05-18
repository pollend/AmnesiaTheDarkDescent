//
// Created by michaelpollind on 5/17/22.
//
#pragma  once

#include <graphics/HPLShaderDefinition.h>

template<class T>
class HPLMaterial {
public:
  HPLMaterial(std::shared_ptr<HPLShaderDefinition<T>> definition)
      : _definition(definition) {
    for(auto& field: _definition->uniforms()) {
      int value = field.get<HPL_PARAMETER_INT>(_data);
    }
  }

  T& data() { return _data; }

private:
  std::shared_ptr<HPLShaderDefinition<T>> _definition;
  T _data;
};