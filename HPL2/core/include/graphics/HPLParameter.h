#pragma once

#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"

#include "IRenderingInc.h"

#include <absl/container/inlined_vector.h>
#include <absl/types/span.h>

namespace hpl {

class HPLMember;

typedef absl::InlinedVector<const HPLMember *, 10> HPLMemberCapture;
typedef absl::Span<const HPLMember> HPLMemberSpan;

enum HPLParameterType {
  HPL_PARAMETER_NONE,
  HPL_PARAMETER_FLOAT,
  HPL_PARAMETER_INT,
  HPL_PARAMETER_BOOL,
  HPL_PARAMETER_VEC2F,
  HPL_PARAMETER_VEC2I
};

enum HPLAccessPattern {
  HPL_UNKNOWN_ACCESS_PATTERN,
  HPL_STATIC_ACCESS_PATTERN,
  HPL_PER_FRAME_ACCESS_PATTERN,
};

template <HPLParameterType T> struct parameter_type : public std::bad_typeid {};
template <> struct parameter_type<HPLParameterType::HPL_PARAMETER_INT> { typedef int type; };
template <> struct parameter_type<HPLParameterType::HPL_PARAMETER_FLOAT> { typedef float type; };
template <> struct parameter_type<HPLParameterType::HPL_PARAMETER_BOOL> { typedef bool type; };

enum HPLMemberType {
  HPL_MEMBER_NONE,
  HPL_MEMBER_STRUCT,
  HPL_MEMBER_TEXTURE,
  HPL_MEMBER_VERTEX_SHADER,
  HPL_MEMBER_PIXEL_SHADER,
  HPL_MEMBER_COUNT
};

template <HPLMemberType T> struct is_shader_type : std::false_type {};
template <> struct is_shader_type<HPLMemberType::HPL_MEMBER_PIXEL_SHADER> : std::true_type {};
template <> struct is_shader_type<HPLMemberType::HPL_MEMBER_VERTEX_SHADER> : std::true_type {};

template <HPLMemberType T> struct is_struct_type : std::false_type {};
template <> struct is_struct_type<HPLMemberType::HPL_MEMBER_STRUCT> : std::true_type {};

template <HPLMemberType T> struct is_texture_type : std::false_type {};
template <> struct is_texture_type<HPLMemberType::HPL_MEMBER_TEXTURE> : std::true_type {};


struct HPLStructParameter {
  HPLParameterType type;
  const char *memberName;
  size_t offset;
  uint16_t number;

};

typedef struct {
public:
  uint32_t bits;
  const char *key;
  const char *value;
} HPLShaderPermutation;

typedef struct {
  const absl::Span<const HPLShaderPermutation> permutations;
//  const HPLShaderPermutation *permutations;
//  size_t permutationCount;
} ShaderMember;

typedef struct {
  size_t offset;
  size_t size;
  const absl::Span<const HPLStructParameter> parameters;

  static const HPLStructParameter& byFieldName(const HPLMember& mm, const std::string& fieldName);
} MemberStruct;

typedef struct {
  size_t offset;
} MemberTexture;

struct HPLMember {
  const char *memberName;
  HPLMemberType type;
  union {
    ShaderMember member_shader;
    MemberStruct member_struct;
    MemberTexture member_texture;
  };

  static size_t getOffset(const HPLMember& member);
  static void byType(const HPLMemberSpan& members, HPLMemberType type, HPLMemberCapture& capture);
  static const HPLMember& byMemberName(const HPLMemberSpan& members, const std::string& memberName);
  static int countByType(const HPLMemberSpan& members, HPLMemberType type);
};

static const HPLMember EMPTY_MEMBER = {.type = HPLMemberType::HPL_MEMBER_NONE};
static const HPLStructParameter EMPTY_STRUCT_PARAMETER = {.type = HPLParameterType::HPL_PARAMETER_NONE};



}