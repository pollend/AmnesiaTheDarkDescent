#include "graphics/HPLParameter.h"
//
//size_t hpl::HPLMember::getOffset(const HPLMember& member) {
//  switch (member.type) {
//  case HPL_MEMBER_STRUCT:
//    return member.member_struct.offset;
//  case HPL_MEMBER_TEXTURE:
//    return member.member_texture.offset;
//  default:
//    break;
//  }
//  return SIZE_MAX;
//}
//
//
//const hpl::HPLStructParameter& hpl::MemberStruct::byFieldName(const HPLMember& member, const std::string& fieldName) {
//  if (member.type == HPL_MEMBER_STRUCT) {
//    for (const auto &p : member.member_struct.parameters) {
//      if(p.memberName == fieldName) {
//        return p;
//      }
//    }
//  }
//  return EMPTY_STRUCT_PARAMETER;
//}
//
//void hpl::HPLMember::byType(const HPLMemberSpan& members, HPLMemberType type, HPLMemberCapture& capture) {
//  for (const HPLMember &member : members) {
//    if (member.type == type) {
//      capture.push_back(&member);
//    }
//  }
//}
//
//const hpl::HPLMember& hpl::HPLMember::byMemberName(const HPLMemberSpan& members, const std::string& memberName) {
//  for (const HPLMember &member : members) {
//    if (member.memberName == memberName) {
//      return member;
//    }
//  }
//  return EMPTY_MEMBER;
//}
//
//int hpl::HPLMember::countByType(const HPLMemberSpan &members, HPLMemberType type) {
//  int result = 0;
//  for (const HPLMember &member : members) {
//    if (member.type == type) {
//      result++;
//    }
//  }
//  return result;
//}