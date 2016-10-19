#pragma once
#include <algorithm>
#include <experimental/optional>
#include "streams_common.hpp"
#include <type_safe/types.hpp>
#include <type_safe/optional.hpp>

namespace streams {
    struct read_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class Istream {
    public:
        Istream() {}
        Istream(const Istream&) = delete;
        Istream(Istream&&) = delete;
        Istream& operator=(const Istream&) = delete;
        Istream& operator=(Istream&&) = delete;
        virtual ~Istream() {}

        type_safe::size_t read(gsl::span<gsl::byte> s)
        { return _read(s); }

        type_safe::optional<gsl::byte> get_byte()
        {
            gsl::byte b;
            auto bytes_read = read({&b, 1});
            if (1u == bytes_read) return b;
            else return type_safe::nullopt;
        }

        //Read binary/unformatted data in host endianess.
        template<typename T>
        type_safe::optional<T> get_data()
        {
            static_assert(std::is_trivially_copyable<T>::value,
                    "Canot use get_data() on values that are not trivially "
                    "copyable.");
            T t;
            gsl::span<T> s{&t, 1};
            auto bytes_read = read(gsl::as_writeable_bytes(s));
            if (span_size_to_safe_size(s.size_bytes()) == bytes_read) return t;
            else return type_safe::nullopt;
        }

        type_safe::optional<std::string> getline()
        {
            std::string line;
            while (true) {
                auto oc = get_data<char>();
                if (!oc) {
                    if (line.empty()) return type_safe::nullopt;
                    else return line;
                }
                if ('\n' == oc.value()) return line;
                line += oc.value();
            }
        }

        //I used this sort of function a lot with std::istream.
        //The current definition of Istream::_read doesn't make it easy and
        //efficitient to implement thought.
        void ignore_bytes(type_safe::size_t n)
        { for (type_safe::size_t i = 0u; i < n; ++i) get_byte(); }

        //std::vector<gsl::byte> read_until(byte);

        //tell and seek?
    private:
        virtual type_safe::size_t _read(gsl::span<gsl::byte>) = 0;
    };

    //TODOs
    //String_istream
    //Filtered_istream
    //Pipe_istream

    template<typename T>
    class Stdio_istream: public Istream {
    public:
        Stdio_istream() {}

        std::FILE* _file() { return static_cast<T*>(this)->_file(); }

    private:
        type_safe::size_t _read(gsl::span<gsl::byte> s) override
        {
            //We have to do this static_assert in a function to delay it until
            //Stdio_istream is a complete type.
            static_assert(std::is_base_of<Stdio_istream, T>::value,
                    "Stdio_istream should only be used with classes derived "   
                    "from itself. See the CRTP.");
            auto bytes_read = std::fread(s.data(), 1, s.size(), _file());
            if (std::ferror(_file())) {
                throw read_error("Error calling fread()");
            }
            return bytes_read;
        }
    };

    class Simple_stdio_istream: public Stdio_istream<Simple_stdio_istream> {
    public:
        explicit Simple_stdio_istream(std::FILE* f): _f(f) {}

        std::FILE* _file() { return _f; }

    private:
        std::FILE* _f;
    };

    Simple_stdio_istream stdins(stdin);

    class File_istream: public Stdio_istream<File_istream> {
    public:
        explicit File_istream(const std::string& path):
            _f(std::fopen(path.c_str(), "r"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* _file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f) { std::fclose(f); }
        };

        std::unique_ptr<FILE, Closer> _f;
    };

    //Buffered_istream
    //Add a buffer to an Istream.
    //Not good for interactive use as it can block waiting to fill up the
    //buffer.
    class Buffered_istream: public Istream {
    public:
        explicit Buffered_istream(
                Istream& source, type_safe::size_t buffer_size = 1024u):
            _source(source), _buffer(static_cast<size_t>(buffer_size)) {}

    private:
        type_safe::size_t _read(gsl::span<gsl::byte> s) override
        {
            if (_eof) return 0u;
            std::ptrdiff_t bytes_delivered = 0;
            //While the caller still wants bytes...
            while (s.size() > 0) {
                //If the buffer is empty, fill it.
                if (_available.size() <= 0) {
                    _available = _buffer;
                    auto bytes_read = _source.read(_available);
                    if (bytes_read < static_cast<size_t>(_available.size())) {
                        _eof = true;
                    }
                    _available = _available.first(
                            safe_size_to_span_size(bytes_read));
                }
                //Copy from buffer to caller.
                auto to_copy = std::min(s.size(), _available.size());
                std::copy_n(_available.begin(), to_copy, s.begin());
                _available = _available.subspan(to_copy);
                s = s.subspan(to_copy);
                bytes_delivered = to_copy;
                if (_eof) break;
            }
            return span_size_to_safe_size(bytes_delivered);
        }

        Istream& _source;
        std::vector<gsl::byte> _buffer;
        gsl::span<gsl::byte> _available;
        bool _eof = false;
    };

    //Unget_istream
    //Enable arbitrary amounts of unget for any Istream.
    //The data you unget doesn't even have to be the same as what you read.
    //You don't even have to have read data previously.
    class Unget_istream: public Istream {
    public:
        explicit Unget_istream(Istream& source): _source(source) {}

        void unget(gsl::span<const gsl::byte> s)
        { std::copy(s.crbegin(), s.crend(), std::back_inserter(_buffer)); }

    private:
        type_safe::size_t _read(gsl::span<gsl::byte> s) override
        {
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
                bytes_given += safe_size_to_span_size(_source.read(s));
            }
            return span_size_to_safe_size(bytes_given);
        }

        Istream& _source;
        std::vector<gsl::byte> _buffer;
    };
}

