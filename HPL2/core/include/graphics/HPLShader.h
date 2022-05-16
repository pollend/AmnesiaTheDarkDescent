#pragma once
#include <memory>
#include <vector>
#include <map>

#include "graphics/HPLParameter.h"
#include "graphics/HPLTexture.h"
#include "system/SystemTypes.h"
#include "resources/Resources.h"
#include "graphics/HPLShader.h"
#include <graphics/HPLParameter.h>

namespace hpl {

class IHPLShader {};

class IHPLMaterialRef {};


//template <typename TData> class HPLMaterialRef : public IHPLMaterialRef {
//public:
//  HPLMaterialRef(HPLMaterial<TData> *target) : _target(target) {}
//  ~HPLMaterialRef() {}
//
//  void init() {
//    void *data = &_data;
//    //        std::vector<DescriptorData> params;
//    for (int i = 0; i < _target->nMembers(); i++) {
//      const HPLMember &member = _target->byIndex(i);
//      //          DescriptorData descriptor;
//      switch (member.type) {
//      case HPL_MEMBER_TEXTURE: {
//        auto &target = member.member_texture;
//        void *memberData = data + target.offset;
//        //        Texture *texture = dynamic_cast<iTexture *>(memberData);
//        //        descriptor.pName = target.memberName;
//        //        descriptor.ppTextures = &texture;
//      } break;
//      case HPL_MEMBER_STRUCT: {
//        auto &target = member.member_struct;
//        void *memberData = data + target.offset;
//        char structData[target.size];
//
//      } break;
//      }
//      //          params.push_back(descriptor);
//    }
//  }
//
////  void updateBuffers(const char *fields) {
////    void *data = &_data;
////    HPLMember *member = _target->byField(fields);
////    if (member != nullptr) {
////      char *memberData = data + member->member_struct.offset;
////    }
////  }
//
//  TData &get() { return _data; }
//
//private:
//  TData _data;
//  HPLMaterial<TData> *_target;
//
//  friend class HPLMaterial<TData>;
//};

template <typename TData> class HPLShader : public IHPLShader {
public:
  HPLShader() {}

protected:
  template <typename TStruct>
  HPLUniformLayout<TData, TStruct> &addUniform(TStruct TData::*field) {
    return *static_cast<HPLUniformLayout<TData, TStruct> *>(
        _uniform
            .emplace_back(
                std::make_unique<HPLUniformLayout<TData, TStruct>>(field))
            .get());
  }

  HPLSamplerLayout<TData> &addSampler(HPLSampler TData::*field) {
    return *_samplers.emplace_back(HPLSamplerLayout<TData>(field));
  }

  HPLShaderStages& getStages() {
    return _stages;
  }

private:
  std::vector<HPLSamplerLayout<TData>> _samplers;
  HPLShaderStages _stages;
  std::vector<std::unique_ptr<IHPLUniformLayout>> _uniform;

  //  Renderer *_render;
};

}

