#pragma once

#include "graphics/GraphicsTypes.h"
#include "graphics/HPLSampler.h"
#include "system/SystemTypes.h"

#include <absl/container/inlined_vector.h>
#include <absl/types/span.h>

namespace hpl {

class HPLMember;
class HPLStructParameter;
class HPLShaderPermutation;
class HPLShaderStage;

typedef absl::InlinedVector<const HPLMember *, 10> HPLMemberCapture;
typedef absl::Span<const HPLMember> HPLMemberSpan;
typedef absl::Span<const HPLStructParameter> HPLParameterSpan;
typedef absl::Span<const HPLShaderStage> HPLShaderStageSpan;
typedef absl::Span<const HPLShaderPermutation> HPLPermutationSpan;

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
  HPL_MEMBER_SHADER,
  HPL_MEMBER_SAMPLER,
  HPL_MEMBER_COUNT
};

enum HPLShaderStageType {
  HPL_VERTEX_SHADER,
  HPL_PIXEL_SHADER
};

//template <HPLMemberType T> struct is_shader_type : std::false_type {};
//template <> struct is_shader_type<HPLMemberType::HPL_MEMBER_SHADER> : std::true_type {};
//
//template <HPLMemberType T> struct is_struct_type : std::false_type {};
//template <> struct is_struct_type<HPLMemberType::HPL_MEMBER_STRUCT> : std::true_type {};
//
//template <HPLMemberType T> struct is_texture_type : std::false_type {};
//template <> struct is_texture_type<HPLMemberType::HPL_MEMBER_TEXTURE> : std::true_type {};
//
//struct HPLStructParameter {
//  HPLParameterType type;
//  const char *memberName;
//  size_t offset;
//  uint16_t number;
//};
//
//struct HPLShaderPermutation {
//  uint32_t bits;
//  const char *key;
//  const char *value;
//};
//
//struct HPLShaderStage {
//  const absl::Span<const HPLShaderPermutation> permutations;
//  const char *shaderName;
//  HPLShaderStageType stageType;
//  const char *entryPointName;
//};
//
//typedef struct {
//  const absl::Span<const HPLShaderStage> stages;
//  int numBindlessTexture;
//} ShaderMember;
//
//typedef struct _SamplerMember{
//  HPLSampler* sampler;
//} SamplerMember;
//
//typedef struct _MemberStruct {
//  size_t offset;
//  size_t size;
//  const absl::Span<const HPLStructParameter> parameters;
//
//  static const HPLStructParameter& byFieldName(const HPLMember& mm, const std::string& fieldName);
//} MemberStruct;
//
//typedef struct {
//  size_t offset;
//} MemberTexture;


template <class T>
class HPLUniformField {
public:
  HPLParameterType _type;
  std::string _name;
  union {
    float T::* flt;
  } _field;

  HPLUniformField(const std::string& name,float T::* field):
     _type(HPL_PARAMETER_FLOAT), _name(name), _field({.flt = field}) {}
};


template<class TData, class TField>
class HPLField {
public:
  HPLField(TField TData::* field): _field(field) {

  }

protected:
  TField TData::* _field;
};

class IHPLUniformLayout {
  virtual HPLParameterType getFieldType(const char* name) = 0;
};

template <class TData, class TStruct>
class HPLUniformLayout : public IHPLUniformLayout, public HPLField<TData, TStruct> {
public:
  HPLUniformLayout(TStruct TData::*field) : HPLField<TData, TStruct>(field) {}

  HPLParameterType getFieldType(const char *name) final {
    for (auto &field : _fields) {
    }
    return HPL_PARAMETER_VEC2F;
  }

  template <typename OutputIt> OutputIt getFields() {
    return std::transform(_fields.begin(), _fields.end(),
                          [](HPLUniformField<TStruct> it) { return it._name; });
  }

  template <typename TType>
  HPLUniformLayout<TData, TStruct> &addField(const std::string &name,
                                             TType TStruct::*target) {
    _fields.push_back(HPLUniformField<TStruct>(name, target));
    return *this;
  }

private:
  const char *_name;
  std::vector<HPLUniformField<TStruct>> _fields;
};

template <class TData>
class HPLSamplerLayout : public HPLField<TData, HPLSampler> {
public:
  HPLSamplerLayout(HPLSampler TData::*field)
      : HPLField<TData, HPLSampler>(field) {}
};


class HPLShaderStages  {
public:
  typedef enum {
    VERTEX_STAGE,
    SHADER_STAGE,
    SHADER_STAGE_COUNT
  } ShaderStage;

  HPLShaderStages() {}
};

//template<class TData>
//class HPLLayout {
//public:
//  template <typename TStruct>
//  HPLUniformLayout<TData, TStruct> &addUniform(TStruct TData::*field) {
//    return *static_cast<HPLUniformLayout<TData, TStruct> *>(
//        _uniform
//            .emplace_back(
//                std::make_unique<HPLUniformLayout<TData, TStruct>>(field))
//            .get());
//  }
//
//  HPLSamplerLayout<TData> &addSampler(HPLSampler TData::*field) {
//    return static_cast<HPLSamplerLayout<TData>>(
//        *_samplers.emplace_back(HPLSamplerLayout<TData>(field)));
//  }
//
//private:
//  std::vector<HPLSamplerLayout<TData>> _samplers;
//  std::vector<std::unique_ptr<IHPLUniformLayout>> _uniform;
//};


//struct HPLMember {
//  const char *memberName;
//  HPLMemberType type;
//  union {
//    SamplerMember member_sampler;
//    ShaderMember member_shader;
//    MemberStruct member_struct;
//    MemberTexture member_texture;
//  };
//
//  static size_t getOffset(const HPLMember& member);
//  static void byType(const HPLMemberSpan& members, HPLMemberType type, HPLMemberCapture& capture);
//  static const HPLMember& byMemberName(const HPLMemberSpan& members, const std::string& memberName);
//  static int countByType(const HPLMemberSpan& members, HPLMemberType type);
//};

//static const HPLMember EMPTY_MEMBER = {.type = HPLMemberType::HPL_MEMBER_NONE };
//static const HPLStructParameter EMPTY_STRUCT_PARAMETER = {.type = HPLParameterType::HPL_PARAMETER_NONE};

}