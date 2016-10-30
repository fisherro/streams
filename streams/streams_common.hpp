#pragma once
#include <gsl/gsl>

namespace streams {
    template<typename T>
        constexpr std::ptrdiff_t size(const T& t) { return t.size(); }

    struct seek_error: public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class seekable {
    public:
        enum class seek_origin { set, cur, end };

        void seek(std::ptrdiff_t offset, seek_origin origin)
        { _seek(offset, origin); }

        std::ptrdiff_t tell()
        { return _tell(); }

    private:
        virtual void _seek(std::ptrdiff_t offset, seek_origin origin) = 0;
        virtual std::ptrdiff_t _tell() = 0;
    };

    template<typename T>
    class stdio_seekable: public seekable {
    private:
        //TODO: static_if that T is derived from this.
        //TODO: static_if that T has fd().
        std::FILE* _file() { return static_cast<T*>(this)->file(); }

        void _seek(std::ptrdiff_t offset, seek_origin origin) override
        {
            int o = (seek_origin::set == origin)? SEEK_SET:
                (seek_origin::cur == origin)? SEEK_CUR:
                SEEK_END;
            auto result = std::fseek(_file(), offset, o);
            if (0 != result) throw seek_error("fseek() failed");
        }

        std::ptrdiff_t _tell() override
        {
            auto pos = std::ftell(_file());
            if (EOF == pos) {
                throw std::system_error(errno, std::system_category());
            }
            return pos;
        }
    };

    template<typename T>
    class posix_fd_seekable: public seekable {
    private:
        //TODO: static_if that T is derived from this.
        //TODO: static_if that T has fd().
        int _fd() { return static_cast<T*>(this)->fd(); }

        void _seek(std::ptrdiff_t offset, seek_origin origin) override
        {
            int o = (seek_origin::set == origin)? SEEK_SET:
                (seek_origin::cur == origin)? SEEK_CUR:
                SEEK_END;
            auto loc = lseek(_fd(), offset, o);
            if (-1 == loc) {
                throw std::system_error(errno, std::system_category());
            }
        }

        std::ptrdiff_t _tell() override
        {
            auto loc = lseek(_fd(), 0, SEEK_CUR);
            if (-1 == loc) {
                throw std::system_error(errno, std::system_category());
            }
            return loc;
        }
    };
}

