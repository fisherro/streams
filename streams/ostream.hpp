#pragma once
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>

#include <gsl/gsl>
#include <fmt/format.h>

#include <unistd.h>
#include <fcntl.h>

#include "streams_common.hpp"

//TODO: Split into multiple files.

namespace streams {
    ////////////////////////////////////////////////////////////////////////////
    // ostreams
    ////////////////////////////////////////////////////////////////////////////
    struct flush_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct write_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    //ostream
    //An interface for output streams.
    //
    //To create your own stream, simply subclass and override _write().
    //You may also need to override _flush().
    //
    //If your subclass buffers or manages a data sink (like FILE*), you'll
    //want to include a non-virtual, no-throw flush operation in your dtor.
    class ostream {
    public:
        ostream() {}
        ostream(const ostream&) = delete;
        ostream(ostream&&) = delete;
        ostream& operator=(const ostream&) = delete;
        ostream& operator=(ostream&&) = delete;
        virtual ~ostream() {}

        std::ptrdiff_t write(gsl::span<const gsl::byte> bytes)
        { return _write(bytes); }

        void flush() { _flush(); }

        //Write some binary/unformatted data in host endianess.
        template<typename T>
        void put(const T& t)
        {
            static_assert(std::is_trivially_copyable<T>::value,
                    "Cannot use put_data() on values that are not trivially "
                    "copyable.");
            gsl::span<const T> s{&t, 1};
            _write(gsl::as_bytes(s));
        }

    private:
        virtual std::ptrdiff_t _write(gsl::span<const gsl::byte>) = 0;
        virtual void _flush() {}
    };
    
    //buf_ostream
    //Wrap another ostream and buffer output to it.
    class buf_ostream: public ostream {
    public:
        explicit buf_ostream(ostream& os, std::ptrdiff_t size = 1024):
            _sink(os)
        { _buffer.reserve(size); }

        ~buf_ostream() { no_throw_flush(); }

    private:
        //We need a non-virtual flush to call from the dtor.
        //The virtual _flush will call this too.
        void non_virtual_flush()
        {
            if (!_buffer.empty()) {
                _sink.write(_buffer);
                _buffer.clear();
            }
            _sink.flush();
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

        std::ptrdiff_t _write(gsl::span<const gsl::byte> bytes) override
        {
            auto total = bytes.size();
            std::ptrdiff_t available = _buffer.capacity() - _buffer.size();
            while (bytes.size() > available) {
                auto first = bytes.first(available);
                std::copy(first.begin(), first.end(),
                        std::back_inserter(_buffer));
                flush();
                bytes = bytes.subspan(available);
                available = _buffer.capacity() - _buffer.size();
            }
            std::copy(bytes.begin(), bytes.end(), std::back_inserter(_buffer));
            return total;
        }

        ostream& _sink;
        std::vector<gsl::byte> _buffer;
    };

    class span_ostream: public ostream {
    public:
        explicit span_ostream(gsl::span<gsl::byte> span): _free(span) {}
        gsl::span<gsl::byte> unused() { return _free; }

    private:
        std::ptrdiff_t _write(gsl::span<const gsl::byte> bytes) override
        {
            if (_free.size() <= 0) return 0;
            auto nbytes = std::min(bytes.size(), _free.size());
            std::copy_n(bytes.begin(), nbytes, _free.begin());
            _free = _free.subspan(nbytes);
            return nbytes;
        }

        gsl::span<gsl::byte> _free;
    };

    class vector_ostream: public ostream {
    public:
        std::vector<gsl::byte>& vector() { return _v; }

    private:
        std::ptrdiff_t _write(gsl::span<const gsl::byte> bytes) override
        {
            std::copy(bytes.begin(), bytes.end(), std::back_inserter(_v));
            return bytes.size();
        }

        std::vector<gsl::byte> _v;
    };

    ////////////////////////////////////////////////////////////////////////////
    // formatted output
    ////////////////////////////////////////////////////////////////////////////
    
    //print
    //For using {fmt} with an ostream.
    void print(streams::ostream& os, fmt::CStringRef format, fmt::ArgList args)
    {
        fmt::MemoryWriter w;
        w.write(format, args);
        gsl::span<const char> s{w.data(),
            gsl::narrow<gsl::span<const char>::index_type>(w.size())};
        os.write(gsl::as_bytes(s));
    }
    FMT_VARIADIC(void, print, streams::ostream&, fmt::CStringRef);

    template<typename Char,
        typename Traits = std::char_traits<Char>,
        typename Alloc = std::allocator<Char>>
    void basic_put_string(
            ostream& o,
            const std::basic_string<Char, Traits, Alloc>& s)
    { 
        gsl::span<const Char> span = s;
        o.write(gsl::as_bytes(span));
    }

    template<typename Char, typename Traits = std::char_traits<Char>>
    void basic_put_char(ostream& o, Char c)
    {
        gsl::span<const Char> s{&c, 1};
        o.write(gsl::as_bytes(s));
    }

    template<typename Char,
        typename Traits = std::char_traits<Char>,
        typename Alloc = std::allocator<Char>>
    void basic_put_line(
            ostream& o,
            const std::basic_string<Char, Traits, Alloc>& s)
    {
        gsl::span<const Char> span = s;
        o.write(gsl::as_bytes(span));
        basic_put_char(o, '\n');
    }

    void put_string(ostream& o, const std::string& s)
    { basic_put_string(o, s); }

    void put_line(ostream& o, const std::string& s)
    { basic_put_line(o, s); }

    void put_char(ostream& o, char c)
    { basic_put_char(o, c); }

    void put_wstring(ostream& o, const std::wstring& s)
    { basic_put_string(o, s); }

    void put_wline(ostream& o, const std::wstring& s)
    { basic_put_line(o, s); }

    void put_wchar(ostream& o, wchar_t c)
    { basic_put_char(o, c); }

    ////////////////////////////////////////////////////////////////////////////
    // stdio ostreams
    ////////////////////////////////////////////////////////////////////////////

    //stdio_base_ostream
    //Base class for ostreams using C stdio.
    //
    //To subclass, use the CRTP:
    //  class My_subclass: public stdio_base_ostream<My_subclass>
    //Then provide a file() member function that returns a FILE*.
    template<typename T>
    class stdio_base_ostream: public ostream {
    public:
        stdio_base_ostream() {}

        std::FILE* file() { return static_cast<T*>(this)->file(); }

    private:
        std::ptrdiff_t _write(gsl::span<const gsl::byte> bytes) override
        {
            //We have to do this static_assert in a function to delay it until
            //stdio_base_ostream is a complete type.
            static_assert(std::is_base_of<stdio_base_ostream, T>::value,
                    "stdio_base_ostream should only be used with classes derived "
                    "from itself. See the CRTP.");

            auto bytes_written = std::fwrite(bytes.data(), 1, bytes.size(), file());
            if (std::ferror(file())) {
                throw write_error("Error calling fwrite()");
            }
            return bytes_written;
        }

        void _flush() override
        {
            if (0 != std::fflush(file())) {
                throw flush_error("Error calling fflush()");
            }
        }
    };

    //stdio_ostream
    //A simple stdio_base_ostream that doesn't own its FILE*.
    class stdio_ostream: public stdio_base_ostream<stdio_ostream> {
    public:
        explicit stdio_ostream(std::FILE* f): _f(f) {}
        std::FILE* file() { return _f; }
    private:
        std::FILE* _f;
    };

    //Wrappers for the standard streams:
    stdio_ostream stdouts(stdout);
    stdio_ostream stderrs(stderr);

    //stdio_file_ostream
    //For writing to a file.
    class stdio_file_ostream: public stdio_base_ostream<stdio_file_ostream> {
    public:
        //TODO: Ctor for a std::experimental::filesystem path.
        
        explicit stdio_file_ostream(
                const std::string& path, bool append = false):
            _f(std::fopen(path.c_str(), append? "a": "w"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* file() { return _f.get(); }

        enum class seek_origin { set, cur, end };

        //TODO: Won't work with very large files.
        std::ptrdiff_t tell()
        {
            auto pos = std::ftell(file());
            if (EOF == pos) {
                throw std::system_error(errno, std::system_category());
            }
            return pos;
        }
        
        //TODO: Won't work with very large files.
        void seek(std::ptrdiff_t offset, seek_origin origin)
        {
            int o = SEEK_SET;
            if (seek_origin::cur == origin) o = SEEK_CUR;
            else if (seek_origin::end == origin) o = SEEK_END;
            auto result = std::fseek(file(), offset, o);
            if (0 != result) throw seek_error("fseek() failed");
        }

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

    ////////////////////////////////////////////////////////////////////////////
    // platform specific ostreams
    ////////////////////////////////////////////////////////////////////////////

    //stdio_pipe_ostream
    //Run a command and create a pipe to its stdin.
    //NOTE: Uses popen(), which is not the safest way to start a subprocess.
    //TODO: Need to conditionally compile this in case popen isn't there.
    //
    //Could probably refactor a common base class out of this and File_ostream.
    class stdio_pipe_ostream: public stdio_base_ostream<stdio_pipe_ostream> {
    public:
        explicit stdio_pipe_ostream(const std::string& command):
            _f(popen(command.c_str(), "w"))
        { if (!_f) throw std::system_error(errno, std::system_category()); }

        std::FILE* file() { return _f.get(); }

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

    template<typename T>
    class posix_base_ostream: public ostream {
    public:
        posix_base_ostream() {}
        int fd() { return static_cast<T*>(this)->fd(); }
    private:
        std::ptrdiff_t _write(gsl::span<const gsl::byte> bytes) override
        {
            //We have to do this static_assert in a function to delay it until
            //this is a complete type.
            static_assert(std::is_base_of<posix_base_ostream, T>::value,
                    "posix_base_ostream should only be used with classes "
                    "derived from itself. See the CRTP.");

            auto total_written = 0;
            while (bytes.size() > 0) {
                auto bytes_written = ::write(fd(), bytes.data(), bytes.size());
                if (-1 == bytes_written) {
                    throw std::system_error(errno, std::system_category());
                }
                total_written += bytes_written;
                bytes = bytes.subspan(bytes_written);
            }
            return total_written;
        }
        void _flush() override
        {
            if (-1 == fsync(fd())) {
                throw std::system_error(errno, std::system_category());
            }
        }
    };

    class posix_fd_ostream: public posix_base_ostream<posix_fd_ostream> {
    public:
        explicit posix_fd_ostream(int fd): _fd(fd) {}
        int fd() { return _fd; }
    private:
        int _fd;
    };

    class posix_file_ostream: public posix_base_ostream<posix_file_ostream> {
        static int oflag(bool append)
        { return O_CREAT | O_WRONLY | (append? O_APPEND: O_TRUNC); }

        static mode_t mode()
        { return S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; }

    public:
        explicit posix_file_ostream(
                const std::string& path, bool append = false):
            _fd(open(path.c_str(), oflag(append), mode()))
        {
            if (-1 == _fd) {
                throw std::system_error(errno, std::system_category());
            }
        }

        int fd() { return _fd; }

        //TODO: Factor out seek_origin, tell, and seek into an abstract class?
        std::ptrdiff_t tell()
        {
            auto loc = lseek(_fd, 0, SEEK_CUR);
            if (-1 == loc) {
                throw std::system_error(errno, std::system_category());
            }
            return loc;
        }

        enum class seek_origin { set, cur, end };

        void seek(std::ptrdiff_t offset, seek_origin origin)
        {
            int o = SEEK_SET;
            if (seek_origin::cur == origin) o = SEEK_CUR;
            else if (seek_origin::end == origin) o = SEEK_END;
            auto loc = lseek(_fd, offset, o);
            if (-1 == loc) {
                throw std::system_error(errno, std::system_category());
            }
        }

        ~posix_file_ostream()
        {
            fsync(_fd);
            close(_fd);
        }

    private:
        int _fd;
    };
}

