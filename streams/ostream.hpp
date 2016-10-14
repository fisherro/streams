#pragma once
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <gsl/gsl>
#include <fmt/format.h>

namespace streams {
    struct flush_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct write_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    //Ostream
    //An interface for output streams.
    class Ostream {
    public:
        Ostream() {}
        Ostream(const Ostream&) = delete;
        Ostream(Ostream&&) = delete;
        Ostream& operator=(const Ostream&) = delete;
        Ostream& operator=(Ostream&&) = delete;
        virtual ~Ostream()
        {
            try {
                flush();
            } catch (...) {
                //Don't let exceptions escape dtors!
            }
        }

        size_t write(gsl::span<const gsl::byte> s) { return _write(s); }

        void flush() { _flush(); }

        void put_byte(gsl::byte b)
        { write({&b, 1}); }

        //Write some binary data in host endianess.
        template<typename T>
        void put_data(const T& t)
        {
            gsl::span<const T> s{&t, 1};
            write(gsl::as_bytes(s));
        }

        //tell and seek?
    private:
        virtual size_t _write(gsl::span<const gsl::byte> s) = 0;
        virtual void _flush() {}
    };
    
    //print
    //Use fmt with an Ostream.
    void print(streams::Ostream& os, fmt::CStringRef format, fmt::ArgList args)
    {
        fmt::MemoryWriter w;
        w.write(format, args);
        //os.write({static_cast<const gsl::byte*>(w.data()), w.size()});
        gsl::span<const char> s{w.data(), gsl::narrow<gsl::span<const char>::index_type>(w.size())};
        os.write(gsl::as_bytes(s));
    }
    FMT_VARIADIC(void, print, streams::Ostream&, fmt::CStringRef);

    void prints(streams::Ostream& os, fmt::CStringRef s)
    {
        //I guess we could skip the formatting and directly write it...
        print(os, "{}", s);
    }

    class Buffered_ostream: public Ostream {
    public:
        explicit Buffered_ostream(Ostream& os, size_t size = 1024):
            _stream(os)
        {
            _buffer.reserve(size);
        }

    private:
        size_t _write(gsl::span<const gsl::byte> s) override
        {
            //TODO: Error/exception handling
            auto total = s.size();
            auto available = _buffer.capacity() - _buffer.size();
            while (s.size() > available) {
                auto first = s.first(available);
                std::copy(first.begin(), first.end(),
                        std::back_inserter(_buffer));
                flush();
                s = s.subspan(available);
                available = _buffer.capacity() - _buffer.size();
            }
            std::copy(s.begin(), s.end(), std::back_inserter(_buffer));
            return total;
        }

        void _flush() override
        {
            //TODO: Error/exception handling
            if (!_buffer.empty()) {
                _stream.write(_buffer);
                _stream.flush();
                _buffer.clear();
            }
        }

        Ostream& _stream;
        std::vector<gsl::byte> _buffer;
    };

    class String_ostream: public Ostream {
    public:
        std::string string() { return _string; }

    private:
        size_t _write(gsl::span<const gsl::byte> s) override 
        {
            _string.append(reinterpret_cast<const char*>(s.data()), s.size());
            return s.size();
        }

        std::string _string;
    };

    //Stdio_ostream
    //An output stream for interoperability with C stdio.
    class Stdio_ostream: public Ostream {
    public:
        Stdio_ostream(): _file(nullptr) {}

        explicit Stdio_ostream(std::FILE* f): _file(f) {}

    protected:
        void set_file(std::FILE* f) { _file = f; }
        
    private:
        size_t _write(gsl::span<const gsl::byte> s) override
        {
            auto bytes_written = std::fwrite(s.data(), 1, s.size(), _file);
            if (std::ferror(_file)) {
                throw write_error("Error calling fwrite()");
            }
            return bytes_written;
        }

        void _flush() override
        {
            if (0 != std::fflush(_file)) {
                throw flush_error("Error calling fflush()");
            }
        }

        std::FILE* _file;
    };

    //Wrappers for the standard streams:
    Stdio_ostream stdouts(stdout);
    Stdio_ostream stderrs(stderr);

    //File_ostream
    //For writing to a file.
    class File_ostream: public Stdio_ostream {
    public:
        //File_ostream() {}

        explicit File_ostream(const std::string& path, bool append = false):
            _fp(std::fopen(path.c_str(), append? "a": "w"))
        {
            if (!_fp) {
                throw std::system_error(errno, std::system_category());
            }
            set_file(_fp.get());
        }

    private:
        struct Closer {
            void operator()(std::FILE* f) { std::fclose(f); }
        };

        std::unique_ptr<std::FILE, Closer> _fp;
    };

    //TODO: Need to conditionally compile this in case popen isn't there.
    class Pipe_ostream: public Stdio_ostream {
    public:
        explicit Pipe_ostream(const std::string& command):
            _fp(popen(command.c_str(), "w"))
        {
            if (!_fp) {
                throw std::system_error(errno, std::system_category());
            }
            set_file(_fp.get());
        }

    private:
        struct Closer {
            void operator()(std::FILE* f) { pclose(f); }
        };

        std::unique_ptr<std::FILE, Closer> _fp;
    };

#if 0
    class Filtered_ostream: public Ostream {
    };
#endif
}

