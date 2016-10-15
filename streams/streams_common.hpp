#pragma once
#include <gsl/gsl>
#include <type_safe/types.hpp>

namespace streams {
    constexpr type_safe::size_t span_size_to_safe_size(std::ptrdiff_t x)
    {
        type_safe::ptrdiff_t y = x;
        return type_safe::make_unsigned(y);
    }
}

