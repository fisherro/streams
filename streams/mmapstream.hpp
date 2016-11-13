#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>

namespace streams {
    //Notes for mmap_ostream:
    //O_WRONLY is not sufficient for mmap when writing? Must use O_RDWR?
    //Need to seek to the size you want to write and write a zero there
    //to make the file actually that size? Then mmap it.
    //Extend the file by some fixed amount and then shrink-to-fit when close?
    //Maybe provide a user API to "reserve" like a vector?
    //Would need to unmap on flush?
    //ftruncate()
    //Resize = sync, unmap, "truncate", remap

    //TODO: Handling for large files.
    //TODO: Make seekable.
    class mmap_istream: public istream {
    public:
        explicit mmap_istream(const std::string& path)
        {
            auto fd = open(path.c_str(), O_RDONLY);
            if (-1 == fd) {
                throw std::system_error(errno, std::system_category());
            }
            _fd._fd = fd;

            struct stat info;
            auto result = fstat(fd, &info);
            if (-1 == result) {
                throw std::system_error(errno, std::system_category());
            }
            auto length = info.st_size;

            auto p = mmap(nullptr, length, PROT_READ,
                    MAP_FILE | MAP_PRIVATE, fd, 0);
            if (MAP_FAILED == p) {
                throw std::system_error(errno, std::system_category());
            }

            _mmap.set(reinterpret_cast<gsl::byte*>(p), length);
        }

    private:
        gsl::span<gsl::byte> _read(gsl::span<gsl::byte> bytes) override
        {
            ptrdiff_t bytes_left = _mmap._s - _pos;
            auto length = std::min(bytes_left, bytes.size());
            std::copy_n(_mmap._p + _pos, length, bytes.data());
            _pos += length;
            return bytes.first(length);
        }

        struct Fd {
            int _fd;
            explicit Fd(int fd = -1): _fd(fd) {}
            ~Fd() { if (-1 != _fd) close(_fd); }
        };

        struct Mmap {
            gsl::byte* _p = nullptr;
            size_t _s = 0;
            void set(gsl::byte* p, size_t s) { _p = p; _s = s; };
            ~Mmap() { if (_p) munmap(_p, _s); }
        };

        Fd _fd;
        Mmap _mmap;
        ptrdiff_t _pos = 0;
    };
}

