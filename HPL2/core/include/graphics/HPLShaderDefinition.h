//
// Created by michaelpollind on 5/16/22.
//
#pragma once

#include <absl/container/inlined_vector.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <string>

class HPLSampler;

enum HPLParameterType {
  HPL_PARAMETER_NONE,
  HPL_PARAMETER_FLOAT,
  HPL_PARAMETER_INT,
  HPL_PARAMETER_BOOL,
  HPL_PARAMETER_VEC2F,
  HPL_PARAMETER_VEC2I
};

template<class TData>
union HPLParameterField{
    float TData::*valueFloat;
    int TData::*valueInt;
    bool TData::*valueBool;
};

template <HPLParameterType T> struct HPLParameterMetaType : public std::bad_typeid {};
template <> struct HPLParameterMetaType<HPLParameterType::HPL_PARAMETER_INT> {
  typedef int type;
  template <class TData>
  int get(const HPLParameterField<TData>& field, TData* data) {
    return data->*field.valueInt;
  }

  template <class TData>
  void set(const HPLParameterField<TData>& field, TData* data, int value) {
    (data->*field.valueInt) = value;
  }
};
template <> struct HPLParameterMetaType<HPLParameterType::HPL_PARAMETER_FLOAT> {
  typedef float type;

  template <class TData>
  float get(const HPLParameterField<TData>& field, TData* data) {
    return data->*field.valueFloat;
  }

  template <class TData>
  void set(const HPLParameterField<TData>& field, TData* data, float value) {
    (data->*field.valueFloat) = value;
  }

};
template <> struct HPLParameterMetaType<HPLParameterType::HPL_PARAMETER_BOOL> { typedef bool type; };

class HPLMemberID {
public:
  HPLMemberID(const std::string_view &name)
      : _id(std::hash<std::string_view>{}(name)), _name(name) {}

  [[nodiscard]] const std::string_view &name() const { return _name; }
  [[nodiscard]] size_t id() const { return _id; }

private:
  size_t _id;
  std::string_view _name;
};


class HPLShaderVariant {
public:
  HPLShaderVariant(uint mask, const HPLMemberID &member_id)
      : _id(member_id), _mask(mask) {}

  [[nodiscard]] uint mask() const { return _mask; }
  [[nodiscard]] const HPLMemberID &id() const{ return _id; }

private:
  HPLMemberID _id;
  uint _mask;
};

class IHPLShaderDefinition {

};

class IHPLUniformDefinition {

};

template <typename TData>
class HPLShaderDefinition: public IHPLShaderDefinition {
public:
  template <class TField> class HPLShaderField {
  public:
    HPLShaderField(TField TData::*field) : _field(field) {}

  private:
    TField TData::*_field;
  };

  class HPLSamplerDefinition : public HPLShaderField<HPLSampler> {
  public:
    HPLSamplerDefinition(HPLSampler TData::*field)
        : HPLShaderField<HPLSampler>(field) {}
  };

  class HPLUniformDefinition {
  public:
    explicit HPLUniformDefinition(const HPLMemberID id, float TData::*field)
        : _id(id), _type(HPL_PARAMETER_FLOAT), _field({.valueFloat = field}) {}
    explicit HPLUniformDefinition(const HPLMemberID id, int TData::*field)
        : _id(id), _type(HPL_PARAMETER_INT), _field({.valueInt = field}) {}

    template <HPLParameterType TType>
    std::optional<typename HPLParameterMetaType<TType>::type> get(TData *data) {
      HPLParameterMetaType<TType> field;
      return (TType == _type) ? std::optional{field.get(_field, data)}
                              : std::nullopt;
    }
    template <HPLParameterType TType>
    bool set(TData *data, const typename HPLParameterMetaType<TType>::type& value) {
      HPLParameterMetaType<TType> field;
      if (TType == _type) {
        field.set(_field, data, value);
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

protected:
  template <class TType>
  HPLUniformDefinition &addUniform(const HPLMemberID &id, TType TData::*field) {
    return _uniforms.emplace_back(id, field);
  }

  HPLSamplerDefinition &addSampler(HPLSampler TData::*field) {
    return _samplers.emplace_back(field);
  }

  void addVariant(const HPLShaderVariant &variant) {
    _variants.emplace_back(variant);
  }

  const absl::Span<HPLUniformDefinition> &uniforms() const {
    return absl::Span<const HPLUniformDefinition>(_uniforms.begin(),
                                                  _uniforms.end());
  }

private:
  std::vector<HPLSamplerDefinition> _samplers;
  std::vector<HPLUniformDefinition> _uniforms;
  std::vector<HPLShaderVariant> _variants;
};