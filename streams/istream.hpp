#pragma once
#include <experimental/optional>

namespace streams {
    //Import optional into our namespace for convenience.
    template<typename T>
    using optional = std::experimental::optional<T>;
    constexpr std::experimental::nullopt_t nullopt = std::experimental::nullopt;

    class Istream {
    public:
        Istream() {}
        Istream(const Istream&) = delete;
        Istream(Istream&&) = delete;
        Istream& operator=(const Istream&) = delete;
        Istream& operator=(Istream&&) = delete;
        virtual ~Istream() {}

        size_t read(gsl::span<gsl::byte> s)
        { return _read(s); }

        optional<gsl::byte> get_byte()
        {
            gsl::byte b;
            auto bytes_read = read({&b, 1});
            if (1 == bytes_read) return b;
            else return nullopt;
        }

        //Read binary data in host endianess.
        template<typename T>
        optional<T> get_data()
        {
            T t;
            gsl::span<T> s{&t, 1};
            auto bytes_read = read(gsl::as_writeable_bytes(s));
            if (s.size_bytes() == bytes_read) return t;
            else return nullopt;
        }

        optional<std::string> getline()
        {
            std::string line;
            while (true) {
                auto c = get_data<char>();
                if (!c) {
                    if (line.empty()) return nullopt;
                    else return line;
                }
                if ('\n' == c) return line;
                line += *c;
            }
        }

        //I used this sort of function a lot with std::istream.
        //The current definition of Istream::_read doesn't make it easy and
        //efficitient to implement thought.
        void ignore_bytes(size_t n)
        { for (size_t i = 0; i < n; ++i) get_byte(); }

        //std::vector<gsl::byte> read_until(byte);

        //readline?
        
        //unget? Should be done by Buffered_istream?
        
        //tell and seek?
    private:
        virtual size_t _read(gsl::span<gsl::byte>) = 0;
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
        size_t _read(gsl::span<gsl::byte> s) override
        {
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

