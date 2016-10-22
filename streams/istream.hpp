#pragma once
#include <algorithm>
#include <experimental/optional>
#include "streams_common.hpp"

namespace streams {
    template<typename T>
    using optional = std::experimental::optional<T>;
    constexpr std::experimental::nullopt_t nullopt = std::experimental::nullopt;

    struct read_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class istream {
    public:
        istream() {}
        istream(const istream&) = delete;
        istream(istream&&) = delete;
        istream& operator=(const istream&) = delete;
        istream& operator=(istream&&) = delete;
        virtual ~istream() {}

        //Tries to fill the span with bytes.
        //Returns the subspan that was actually filled.
        gsl::span<gsl::byte> read(gsl::span<gsl::byte> s)
        { return _read(s); }

        //Read binary/unformatted data in host endianess.
        template<typename T>
        optional<T> get()
        {
            static_assert(std::is_trivially_copyable<T>::value,
                    "Canot use get_data() on values that are not trivially "
                    "copyable.");
            T t;
            gsl::span<T> s{&t, 1};
            auto bytes_read = read(gsl::as_writeable_bytes(s));
            if (bytes_read.size() < sizeof(T)) return nullopt;
            return t;
        }

        template<typename T>
        bool get(T& t)
        {
            gsl::span<T> s{&t, 1};
            auto bytes_read = read(gsl::as_writeable_bytes(s));
            return bytes_read.size() == sizeof(T);
        }

        void ignore_bytes(std::ptrdiff_t n)
        {
            Expects(n >= 0);
            for (std::ptrdiff_t i = 0; i < n; ++i) get<gsl::byte>();
        }

        std::vector<gsl::byte> read_until(const gsl::byte sentinel)
        {
            std::vector<gsl::byte> v;
            while (true) {
                auto byte = get<gsl::byte>();
                if (!byte) return v;
                v.push_back(*byte);
                if (sentinel == *byte) return v;
            }
        }

    private:
        virtual gsl::span<gsl::byte> _read(gsl::span<gsl::byte>) = 0;
    };

    class buf_istream: public istream {
    public:
        explicit buf_istream(
                istream& source, std::ptrdiff_t buffer_size = 1024):
            _source(source), _buffer(buffer_size) {}

    private:
        gsl::span<gsl::byte> _read(gsl::span<gsl::byte> s) override
        {
            auto original_span = s;
            if (_eof) return s.first(0);
            std::ptrdiff_t bytes_delivered = 0;
            //While the caller still wants bytes...
            while (s.size() > 0) {
                //If the buffer is empty, fill it.
                if (_available.size() <= 0) {
                    _available = _buffer;
                    _available = _source.read(_available);
                    if (_available.size() < _buffer.size()) {
                        //If we didn't fill the buffer..
                        _eof = true;
                    }
                }
                //Copy from buffer to caller.
                auto to_copy = std::min(s.size(), _available.size());
                std::copy_n(_available.begin(), to_copy, s.begin());
                _available = _available.subspan(to_copy);
                s = s.subspan(to_copy);
                bytes_delivered += to_copy;
                if (_eof) break;
            }
            return original_span.first(bytes_delivered);
        }

        istream& _source;
        std::vector<gsl::byte> _buffer;
        gsl::span<gsl::byte> _available;
        bool _eof = false;
    };

    class span_istream: public istream {
    public:
        explicit span_istream(gsl::span<const gsl::byte> s): _available(s) {}

    private:
        gsl::span<gsl::byte> _read(gsl::span<gsl::byte> s) override
        {
            auto nbytes = std::min(s.size(), _available.size());
            std::copy_n(_available.begin(), nbytes, s.begin());
            _available = _available.subspan(nbytes);
            return s.first(nbytes);
        }

        gsl::span<const gsl::byte> _available;
    };

    //unget_istream
    //Enable arbitrary amounts of unget for any istream.
    //The data you unget doesn't even have to be the same as what you read.
    //You don't even have to have read data previously.
    class unget_istream: public istream {
    public:
        explicit unget_istream(istream& source): _source(source) {}

        void unget(gsl::span<const gsl::byte> s)
        { std::copy(s.crbegin(), s.crend(), std::back_inserter(_buffer)); }

    private:
        gsl::span<gsl::byte> _read(gsl::span<gsl::byte> s) override
        {
            auto original_span = s;
            std::ptrdiff_t bytes_given = 0;
            if (!_buffer.empty()) {
                std::ptrdiff_t buf_size = _buffer.size();
                auto to_copy = std::min(s.size(), buf_size);
                std::copy_n(_buffer.crbegin(), to_copy, s.begin());
                _buffer.resize(buf_size - to_copy);
                s = s.subspan(to_copy);
                bytes_given = to_copy;
            }
            if (s.size() > 0) {
                auto span_read = _source.read(s);
                bytes_given += span_read.size();
            }
            return original_span.first(bytes_given);
        }

        istream& _source;
        std::vector<gsl::byte> _buffer;
    };

    template<typename C, typename T = std::char_traits<C>>
    optional<C> basic_get_char(istream& in)
    { return in.get<C>(); }

    template<typename C,
        typename T = std::char_traits<C>,
        typename A = std::allocator<C>>
    optional<std::basic_string<C, T, A>>
        basic_get_line(istream& in, C nl = '\n')
    {
        auto c = basic_get_char<C, T>(in);
        if (!c) return nullopt;
        std::basic_string<C, T, A> s;
        while (true) {
            if (nl == *c) return s;
            s += *c;
            c = basic_get_char<C, T>(in);
            if (!c) return s;
        }
    }

    optional<char> get_char(istream& in) { return basic_get_char<char>(in); }
    optional<std::string> get_line(istream& in, char nl = '\n')
    { return basic_get_line<char>(in, nl); }

    template<typename T>
    class stdio_base_istream: public istream {
    public:
        stdio_base_istream() {}

        std::FILE* file() { return static_cast<T*>(this)->file(); }

    private:
        gsl::span<gsl::byte> _read(gsl::span<gsl::byte> s) override
        {
            //We have to do this static_assert in a function to delay it until
            //stdio_base_istream is a complete type.
            static_assert(std::is_base_of<stdio_base_istream, T>::value,
                    "stdio_base_istream should only be used with classes "
                    "derived from itself. See the CRTP.");
            auto bytes_read = std::fread(s.data(), 1, s.size(), file());
            if (std::ferror(file())) {
                throw read_error("Error calling fread()");
            }
            return s.first(bytes_read);
        }
    };

    class stdio_istream: public stdio_base_istream<stdio_istream> {
    public:
        explicit stdio_istream(std::FILE* f): _f(f) {}

        std::FILE* file() { return _f; }

    private:
        std::FILE* _f;
    };

    stdio_istream stdins(stdin);

    class stdio_file_istream: public stdio_base_istream<stdio_file_istream> {
    public:
        explicit stdio_file_istream(const std::string& path):
            _f(std::fopen(path.c_str(), "r"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f) { std::fclose(f); }
        };

        std::unique_ptr<FILE, Closer> _f;
    };

    class stdio_pipe_istream: public stdio_base_istream<stdio_pipe_istream> {
    public:
        explicit stdio_pipe_istream(const std::string& command):
            _f(popen(command.c_str(), "r"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f) { pclose(f); }
        };

        std::unique_ptr<FILE, Closer> _f;
    };
}

