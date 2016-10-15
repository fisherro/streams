#pragma once
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <gsl/gsl>
#include <fmt/format.h>

//TODO: Remove the unsigned ints? GSL uses ptrdiff_t for size.
//TODO: Use foonathan's type_safe library?

namespace streams {
    struct flush_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct write_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    //Ostream
    //An interface for output streams.
    //
    //To create your own stream, simply subclass and override _write().
    //You may also need to override _flush().
    //
    //If your subclass buffers or manages a data sink (like FILE*), you'll
    //want to include a non-virtual, no-throw flush operation in your dtor.
    class Ostream {
    public:
        Ostream() {}
        Ostream(const Ostream&) = delete;
        Ostream(Ostream&&) = delete;
        Ostream& operator=(const Ostream&) = delete;
        Ostream& operator=(Ostream&&) = delete;
        virtual ~Ostream() {}

        size_t write(gsl::span<const gsl::byte> s) { return _write(s); }

        void flush() { _flush(); }

        void put_byte(gsl::byte b)
        { write({&b, 1}); }

        //Write some binary data in host endianess.
        template<typename T>
        void put_data(const T& t)
        {
            //TODO: Place restrictions on types that can be used?
            //(via enable_if or static_assert or ...)
            gsl::span<const T> s{&t, 1};
            write(gsl::as_bytes(s));
        }

        //Worth having this? For times when you have to write some padding.
        template<typename T>
        void put_data_n(const T& t, size_t n)
        {
            //TODO: Should there be a repeat_n algorithm?
            for (size_t i = 0; i < n; ++i) put_data(t);
        }

        //tell and seek?
    private:
        virtual size_t _write(gsl::span<const gsl::byte> s) = 0;
        virtual void _flush() {}
    };
    
    //print
    //For using {fmt} with an Ostream.
    void print(streams::Ostream& os, fmt::CStringRef format, fmt::ArgList args)
    {
        fmt::MemoryWriter w;
        w.write(format, args);
        gsl::span<const char> s{w.data(), gsl::narrow<gsl::span<const char>::index_type>(w.size())};
        os.write(gsl::as_bytes(s));
    }
    FMT_VARIADIC(void, print, streams::Ostream&, fmt::CStringRef);

    void prints(streams::Ostream& os, fmt::CStringRef s)
    {
        //We could skip {fmt} and write directly.
        print(os, "{}", s);
    }

    //Buffered_ostream
    //Wrap another Ostream and buffer output to it.
    class Buffered_ostream: public Ostream {
    public:
        explicit Buffered_ostream(Ostream& os, size_t size = 1024):
            _stream(os)
        { _buffer.reserve(size); }

        ~Buffered_ostream() { no_throw_flush(); }

    private:
        //We need a non-virtual flush to call from the dtor.
        //The virtual _flush will call this too.
        void non_virtual_flush()
        {
            if (!_buffer.empty()) {
                _stream.write(_buffer);
                _buffer.clear();
            }
            _stream.flush();
        }

        //Needed for flushing from dtor.
        void no_throw_flush() noexcept
        {
            try { non_virtual_flush(); }
            catch (...) {}
        }

        void _flush() override
        {
            non_virtual_flush();
        }

        size_t _write(gsl::span<const gsl::byte> s) override
        {
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

        Ostream& _stream;
        std::vector<gsl::byte> _buffer;
    };

    //String_ostream
    //Like std::ostringstream.
    //fmt::format() can be used instead in many cases.
    //It might be more useful to have a std::vector<gsl::byte> ostream.
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
    //Base class for Ostreams using C stdio.
    //
    //To subclass, use the CRTP:
    //  class My_subclass: public Stdio_ostream<My_subclass>
    //Then provide a _file() member function that returns a FILE*.
    template<typename T>
    class Stdio_ostream: public Ostream {
    public:
        Stdio_ostream() {}

        //I have to make this public?
        std::FILE* _file() { return static_cast<T*>(this)->_file(); }

    private:
        size_t _write(gsl::span<const gsl::byte> s) override
        {
            auto bytes_written = std::fwrite(s.data(), 1, s.size(), _file());
            if (std::ferror(_file())) {
                throw write_error("Error calling fwrite()");
            }
            return bytes_written;
        }

        void _flush() override
        {
            if (0 != std::fflush(_file())) {
                throw flush_error("Error calling fflush()");
            }
        }
    };

    //Simple_stdio_ostream
    //A simple Stdio_ostream that doesn't own its FILE*.
    class Simple_stdio_ostream: public Stdio_ostream<Simple_stdio_ostream> {
    public:
        explicit Simple_stdio_ostream(std::FILE* f): _f(f) {}
        std::FILE* _file() { return _f; }
    private:
        std::FILE* _f;
    };

    //Wrappers for the standard streams:
    Simple_stdio_ostream stdouts(stdout);
    Simple_stdio_ostream stderrs(stderr);

    //File_ostream
    //For writing to a file.
    class File_ostream: public Stdio_ostream<File_ostream> {
    public:
        explicit File_ostream(const std::string& path, bool append = false):
            _f(std::fopen(path.c_str(), append? "a": "w"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* _file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f)
            {
                std::fflush(f);
                std::fclose(f);
            }
        };

        std::unique_ptr<std::FILE, Closer> _f;
    };

    //Pipe_ostream
    //Run a command and create a pipe to its stdin.
    //NOTE: Uses popen(), which is not the safest way to start a subprocess.
    //TODO: Need to conditionally compile this in case popen isn't there.
    //
    //Could probably refactor a common base class out of this and File_ostream.
    class Pipe_ostream: public Stdio_ostream<Pipe_ostream> {
    public:
        explicit Pipe_ostream(const std::string& command):
            _f(popen(command.c_str(), "w"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* _file() { return _f.get(); }

    private:
        struct Closer {
            void operator()(std::FILE* f)
            {
                fflush(f);
                pclose(f);
            }
        };

        std::unique_ptr<std::FILE, Closer> _f;
    };

#if 0
    //Do we need this class to make it easier to created ostream filters?
    class Filtered_ostream: public Ostream {
    };
#endif
}

