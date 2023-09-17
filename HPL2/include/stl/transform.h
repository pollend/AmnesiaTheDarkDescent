#pragma once

#include <functional>
#include <optional>
#include <utility>

namespace hpl::stl {
    template<typename T, typename R, typename Handler>
    R TransformOrDefault(T input, R value, Handler handler) {
        using result_t = std::decay_t<decltype(handler(input.value()))>;
        return input.has_value() ? handler(input.value()): static_cast<result_t>(value);
    }

}
