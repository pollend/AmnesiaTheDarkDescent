#pragma once

#include <Common_3/Renderer/IRenderer.h>
#include <Common_3/Renderer/IResourceLoader.h>
#undef DestroyAll
#undef ButtonPress
#include <memory>
#include <vector>
#include <map>

#include "graphics/HPLParameter.h"
#include "graphics/HPLTexture.h"
#include "system/SystemTypes.h"
#include "resources/Resources.h"
#include "graphics/HPLShader.h"

namespace hpl {

class IHPLMaterial {};

class IHPLMaterialRef {};

template <typename> class HPLMaterial;

template <typename TData> class HPLMaterialRef : public IHPLMaterialRef {
public:
  HPLMaterialRef(HPLMaterial<TData> *target) : _target(target) {}
  ~HPLMaterialRef() {}

  void init() {
    void *data = &_data;
    std::vector<DescriptorData> params;
    for (int i = 0; i < _target->nMembers(); i++) {
      const HPLMember &member = _target->byIndex(i);
      DescriptorData descriptor;
      switch (member.type) {
      case HPL_MEMBER_TEXTURE: {
        auto &target = member.member_texture;
        void *memberData = data + target.offset;
//        Texture *texture = dynamic_cast<iTexture *>(memberData);
//        descriptor.pName = target.memberName;
//        descriptor.ppTextures = &texture;
      } break;
      case HPL_MEMBER_STRUCT: {
        auto &target = member.member_struct;
        void *memberData = data + target.offset;
        char structData[target.size];

      } break;
      }
      params.push_back(descriptor);
    }
  }

  void updateBuffers(const char *fields) {
    void *data = &_data;
    HPLMember *member = _target->byField(fields);
    if (member != nullptr) {
      char *memberData = data + member->member_struct.offset;
    }
  }

  TData &get() { return _data; }

private:
  TData _data;
  HPLMaterial<TData> *_target;

  friend class HPLMaterial<TData>;
};

template <typename TData> class HPLMaterial : public IHPLMaterial {
public:
  HPLMaterial(cResources* resource, Renderer *render, const HPLMember *members, int nMembers)
      : _resources(resource), _members(members), _memberCount(nMembers), _render(render) {}


  std::unique_ptr<HPLMaterialRef<TData>> createRef(uint32_t features) {
    return std::make_unique<TData>(this, nullptr);
  }

  const HPLMember& byField(const char *field) {
    for (HPLMember &m : members()) {
      if (strcmp(m.memberName, field) != 0) {
        return m;
      }
    }
  }

   std::vector<HPLMember> members() {
     return std::vector<HPLMember>{_members, &_members[_memberCount]};
   }

  const HPLMember &byIndex(size_t index) { return _members[index]; }
  size_t nMembers() { return _memberCount; }

protected:
  cResources* _resources;
  const HPLMember *_members;
  const int _memberCount;
  Renderer *_render;
};



}

