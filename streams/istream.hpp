#pragma once
#include <experimental/optional>
#include "streams_common.hpp"
#include <type_safe/types.hpp>
#include <type_safe/optional.hpp>

//TODO: I got this error. I had to #if 0 out the std::hash specialization in
//type_safe/optional.hpp.
//type_safe/include/type_safe/optional.hpp:1039:11: error: explicit specialization of non-template class 'hash'

namespace streams {
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

        //readline?
        
        //unget? Should be done by Buffered_istream?
        
        //tell and seek?
    private:
        virtual type_safe::size_t _read(gsl::span<gsl::byte>) = 0;
    };

    //TODOs
    //String_istream
    //Buffered_istream
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
            return std::fread(s.data(), 1, s.size(), _file());
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
        File_istream(const std::string& path):
            _f(std::fopen(path.c_str(), "r"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* _file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f) { std::fclose(f); }
        };

        std::unique_ptr<FILE, Closer> _f;
    };
}

