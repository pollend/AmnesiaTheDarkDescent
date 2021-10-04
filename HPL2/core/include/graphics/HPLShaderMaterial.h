#pragma once
#include "HPLShader.h"

template <class TMemberLayout>
class HPLShaderMaterial: public HPLShader<TMemberLayout> {
  void setUpdatePattern(const std::string& memberName){

  }
  void updateMember(const std::string& memberName) {

  }
};