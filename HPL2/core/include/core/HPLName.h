#pragma once

#include <string_view>

class HPLName {
public:
  HPLName(const std::string_view &name)
      : _id(std::hash<std::string_view>{}(name)), _name(name) {}

  [[nodiscard]] const std::string_view &name() const { return _name; }
  [[nodiscard]] size_t id() const { return _id; }

private:
  size_t _id;
  std::string_view _name;
};
