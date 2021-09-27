#pragma once

#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"


namespace hpl {

enum HPLMemberType {
  HPL_MEMBER_NONE,
  HPL_MEMBER_STRUCT,
  HPL_MEMBER_PARAMETER,
  HPL_MEMBER_TEXTURE,
  HPL_MEMBER_TEXTURE_3D,
  HPL_MEMBER_VERTEX_SHADER,
  HPL_MEMBER_PIXEL_SHADER,
  HPL_MEMBER_COUNT
};

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
  const char *name;
  const HPLShaderPermutation *permutations;
  size_t permutationCount;
} ShaderMember;

typedef struct {
  const char *memberName;
  size_t offset;
  size_t size;
  const HPLStructParameter *parameters;
  size_t parameterCount;
} MemberStruct;

typedef struct {
  const char *memberName;
  size_t offset;
} MemberTexture;

struct HPLMember {
  HPLMemberType type;
  union {
    ShaderMember pixel_shader;
    ShaderMember vertex_shader;
    MemberStruct member_struct;
    MemberTexture member_texture;
  };
};

}