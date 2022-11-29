
//
// Created by michaelpollind on 5/16/22.
//
#pragma once

#include "absl/types/span.h"
#include <absl/container/inlined_vector.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>

namespace hpl::shader::definition {

enum HPLParameterType {
  HPL_PARAMETER_NONE,
  HPL_PARAMETER_FLOAT,
  HPL_PARAMETER_INT,
  HPL_PARAMETER_BOOL,
  HPL_PARAMETER_VEC2F,
  HPL_PARAMETER_VEC2I
};

// template<typename TType, typename TData>
// class HPLMapper {
// public:
//   using MappingGetter = std::function<TType(const TData& data)>;
//   using MappingSetter = std::function<void(TData& data, TType)>;

//   HPLMapper(MappingGetter getter, MappingSetter setter) : getter(getter), setter(setter) {}
//   HPLMapper() = default;
//   ~HPLMapper() = default;

//   MappingGetter getter;
//   MappingSetter setter;
// };

template<class TData>
class HPLParameterField;


template <HPLParameterType T> struct HPLParameterTrait : public std::bad_typeid {};
template <> struct HPLParameterTrait<HPLParameterType::HPL_PARAMETER_INT> {
public:
  using Type = int; 
  template <class TData>
  void Set(const HPLParameterField<TData>& field, TData& data, int value) {
    return field.intMapper(data, value);
  }
};

template <> struct HPLParameterTrait<HPLParameterType::HPL_PARAMETER_FLOAT> {
  using Type = float;
  template <class TData>
  void Set(const HPLParameterField<TData>& field, TData& data, float value) {
    field.floatMapper(data, value);
  }
};
template <> struct HPLParameterTrait<HPLParameterType::HPL_PARAMETER_BOOL> { typedef bool Type; };


template<class TData>
class HPLParameterField {
public:
  template<HPLParameterType TType>
  struct MappedTypes {
    using Setter = std::function<void(TData& data, typename HPLParameterTrait<TType>::Type)>;
  };

  using IntMapper = typename MappedTypes<HPLParameterType::HPL_PARAMETER_INT>::Setter;
  using FloatMapper = typename MappedTypes<HPLParameterType::HPL_PARAMETER_FLOAT>::Setter;
  using BoolMapper = typename MappedTypes<HPLParameterType::HPL_PARAMETER_BOOL>::Setter;

  IntMapper intMapper;
  FloatMapper floatMapper;
  BoolMapper boolMapper;
};


class HPLMemberID {
public:
  // static constexpr size_t GetHash(const char* name) {
  //   return std::hash<std::string_view>{}(name);
  // }

  HPLMemberID(const std::string_view &name)
      : _id(std::hash<std::string_view>{}(name)), _name(name) {}
  HPLMemberID(const std::string_view &name, size_t id)
      : _id(id), _name(name) {}

  const std::string_view &name() const { return _name; }
  size_t id() const { return _id; }

private:
  size_t _id;
  std::string_view _name;
};

template <typename TData>
class HPLShaderDefinition final {
public:
  using ParameterField = HPLParameterField<TData>;

  class HPLUniformDefinition {
  public:

    explicit HPLUniformDefinition(const HPLMemberID id, typename ParameterField::IntMapper intMapper)
        : _id(id), _type(HPL_PARAMETER_FLOAT) {
          _field.intMapper = intMapper;
        }
    // explicit HPLUniformDefinition(const HPLMemberID id, HPLMapper<float, TData> floatMapper)
    //     : _id(id), _type(HPL_PARAMETER_INT), _field({.floatMapper = floatMapper}) {}

    ~HPLUniformDefinition() {
    }

    template <HPLParameterType TType>
    bool Set(TData& data, const typename HPLParameterTrait<TType>::Type& value) {
      HPLParameterTrait<TType> field;
      if (TType == _type) {
        field.Set(_field, data, value);
        return true;
      }
      return false;
    }

    HPLParameterType getType() {
      return _type;
    }

    HPLMemberID& getId() { return _id; }

  private:
    HPLMemberID _id;
    HPLParameterType _type;
    HPLParameterField<TData> _field;
  };

  HPLShaderDefinition(std::initializer_list<HPLUniformDefinition> uniforms) : _uniforms(uniforms) {

  }

  const absl::Span<HPLUniformDefinition> Uniforms() const {
    return absl::Span<HPLUniformDefinition>(const_cast<HPLUniformDefinition*>(_uniforms.data()), _uniforms.size());
  }

private:
  std::vector<HPLUniformDefinition> _uniforms;
};

}