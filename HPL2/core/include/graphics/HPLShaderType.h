#pragma once

template <class TMemberLayout>
class HPLShaderType {
public:
  HPLShaderType();
  ~HPLShaderType();

private:
  TMemberLayout _layout;
};