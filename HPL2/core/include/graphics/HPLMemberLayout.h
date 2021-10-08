#pragma once

#include "HPLParameter.h"
#include <absl/container/inlined_vector.h>

namespace hpl {

class IHPLMemberLayout {
public:
  virtual const HPLMember &byMemberName(const std::string &field) const = 0;
  virtual void byType(HPLMemberType type, HPLMemberCapture &list) const = 0;
  virtual int countByType(HPLMemberType type) const = 0;
  virtual const absl::Span<const HPLMember> &members() const = 0;
};


template <typename TParameter> class HPLMemberLayout : public IHPLMemberLayout  {
public:
  using TParamType = TParameter;

  HPLMemberLayout(const HPLMemberLayout<TParameter>& layout)
      : _members(layout._members), _copy(layout._copy){
  }
  HPLMemberLayout(const HPLMember *members, size_t len)
      : _members(members, len) {}
  HPLMemberLayout(const absl::Span<const HPLMember> &members)
      : _members(members) {}
  HPLMemberLayout(const std::vector<HPLMember> &members)
      : _copy(members), _members(_copy) {}


  const HPLMember &byMemberName(const std::string &field) const override {
    return HPLMember::byMemberName(members(), field);
  }
  void byType(HPLMemberType type, HPLMemberCapture &list) const override {
    HPLMember::byType(members(), type, list);
  }
  int countByType(HPLMemberType type) const override {
    return HPLMember::countByType(members(), type);
  }

  const absl::Span<const HPLMember> &members() const override { return _members; }

  template <HPLParameterType TType>
  bool getParameter(TParameter *parameter, const std::string &memberName,
                    const std::string &fieldName,
                    typename hpl::parameter_type<TType>::type **value) {
      auto &member = byMemberName(memberName);
      if (member.type != HPL_MEMBER_STRUCT) {
          return false;
      }
      auto &field = MemberStruct::byFieldName(member, fieldName);
      if (field.type != TType) {
          return false;
      }
      uint8_t *data =
              ((uint8_t *) parameter) + member.member_struct.offset + field.offset;
      (*value) = (typename hpl::parameter_type<TType>::type *) data;

      return true;
  }


private:
  std::vector<HPLMember> _copy;
  const HPLMemberSpan _members;

};

}