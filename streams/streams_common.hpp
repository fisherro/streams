#pragma once
#include <gsl/gsl>
#include <type_safe/types.hpp>

namespace streams {
    constexpr type_safe::size_t span_size_to_safe_size(std::ptrdiff_t x)
    {
        type_safe::ptrdiff_t y = x;
        return type_safe::make_unsigned(y);
    }

    constexpr std::ptrdiff_t safe_size_to_span_size(type_safe::size_t x)
    {
        return static_cast<ptrdiff_t>(type_safe::make_signed(x));
    }
}

