#pragma once
#include <gsl/gsl>

namespace streams {
    template<typename T>
        constexpr std::ptrdiff_t size(const T& t) { return t.size(); }

    struct seek_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

