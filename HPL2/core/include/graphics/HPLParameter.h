#pragma once

#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include <absl/types/span.h>

namespace hpl {

enum HPLMemberType {
  HPL_MEMBER_NONE,
  HPL_MEMBER_STRUCT,
  HPL_MEMBER_TEXTURE,
  HPL_MEMBER_TEXTURE_3D,
  HPL_MEMBER_VERTEX_SHADER,
  HPL_MEMBER_PIXEL_SHADER,
  HPL_MEMBER_COUNT
};

template<HPLMemberType T> struct is_shader_type: std::false_type {};
template<> struct is_shader_type<HPLMemberType::HPL_MEMBER_PIXEL_SHADER>: std::true_type {};
template<> struct is_shader_type<HPLMemberType::HPL_MEMBER_VERTEX_SHADER>: std::true_type {};

template<HPLMemberType T> struct is_struct_type: std::false_type {};
template<> struct is_struct_type<HPLMemberType::HPL_MEMBER_STRUCT>: std::true_type {};


enum HPLParameterType {
  HPL_PARAMETER_NONE,
  HPL_PARAMETER_FLOAT,
  HPL_PARAMETER_BOOL,
  HPL_PARAMETER_INTEGER,
  HPL_PARAMETER_VEC2F,
  HPL_PARAMETER_VEC2I
};

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
  const HPLShaderPermutation *permutations;
  size_t permutationCount;
} ShaderMember;

typedef struct {
  size_t offset;
  size_t size;
  const HPLStructParameter *parameters;
  size_t parameterCount;
} MemberStruct;

typedef struct {
  size_t offset;
} MemberTexture;

struct HPLMember {
  const char *memberName;
  HPLMemberType type;
  union {
    ShaderMember shader;
    MemberStruct member_struct;
    MemberTexture member_texture;
  };
};




}