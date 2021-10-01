#pragma once

#include "HPLParameter.h"

namespace hpl {
static const HPLMember EMPTY_MEMBER = {.type = HPLMemberType::HPL_MEMBER_NONE};

typedef std::function<void(const char* shaderName,const ShaderMember &)> ShaderMemberHandler;
typedef std::function<void(const char* shaderName,const MemberStruct &)> StructMemberHandler;

class IHPLMemberLayout {
public:
  template <HPLMemberType TType, typename = typename std::enable_if<
                                     is_shader_type<TType>::value>::type>
  void byType(ShaderMemberHandler handler) {
    findType(TType, [&handler](const HPLMember field) {
      handler(field.memberName, field.member_shader);
    });
  }

  template <HPLMemberType TType, typename = typename std::enable_if<
                                     is_struct_type<TType>::value>::type>
  void byType(StructMemberHandler handler) {
    findType(TType, [&handler](const HPLMember field) {
      handler(field.memberName, field.member_struct);
    });
  }

  virtual int countByType(HPLMemberType type) = 0;
  virtual int countByField(const std::string &field) = 0;

  virtual const HPLMember &byField(const std::string &field) = 0;

  virtual const absl::Span<const HPLMember> &members() = 0;

protected:
  virtual void findType(HPLMemberType type,
                        std::function<void(const HPLMember &)> handler) = 0;
};

template <typename TParameter> class HPLMemberLayout : public IHPLMemberLayout {
public:

  HPLMemberLayout(const HPLMember* members, size_t len) : _members(members, len) {}
  HPLMemberLayout(const absl::Span<HPLMember> &members) : _members(members) {}
  HPLMemberLayout(const std::vector<HPLMember> &members)
      : _copy(members), _members(_copy) {}

  int countByType(HPLMemberType type) override {
    int result = 0;
    for (const HPLMember &member : members()) {
      if (member.type == type) {
        result++;
      }
    }
    return result;
  }

  int countByField(const std::string &field) override {
    int result = 0;
    for (const HPLMember &member : members()) {
      if (member.memberName == field) {
        result++;
      }
    }
    return result;
  }

  TParameter &get() { return _params; }

  void findType(HPLMemberType type,
                std::function<void(const HPLMember &)> handler) override {
    std::vector<HPLMember> result;
    for (const HPLMember &member : members()) {
      if (member.type == type) {
        handler(member);
      }
    }
  }

  const HPLMember &byField(const std::string &field) override {
    for (const HPLMember &member : members()) {
      if (field == member.memberName) {
        return member;
      }
    }
    return EMPTY_MEMBER;
  };

  virtual const absl::Span<const HPLMember> &members() override { return _members; }

private:
  TParameter _params;
  std::vector<HPLMember> _copy;
  const absl::Span<const HPLMember> _members;
};

}