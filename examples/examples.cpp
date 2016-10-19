#include <ctime>
#include <fmt/time.h>
#include <streams/ostream.hpp>
#include <streams/istream.hpp>

std::string encode(gsl::span<const gsl::byte> s)
{
    std::string output;
    for (auto b: s) {
        auto c = static_cast<int>(b);
        if (::isprint(c)) output += static_cast<char>(c);
        else output += fmt::format("[{:02x}]", c);
    }
    return output;
}

struct Debug_ostream: public streams::Ostream {
    std::string _name;
    streams::Ostream& _sink;
    Debug_ostream(const std::string& name, streams::Ostream& stream):
        _name(name), _sink(stream) {}
    type_safe::size_t _write(gsl::span<const gsl::byte> s) override
    {
        streams::print(streams::stderrs, "{}: _write({})\n",
                _name, encode(s));
        return _sink.write(s);
    }
    void _flush() override
    {
        streams::print(streams::stderrs, "{}: _flush()\n", _name);
        _sink.flush();
    }
};

struct Debug_istream: public streams::Istream {
    bool _enabled = true;
    std::string _name;
    streams::Istream& _source;
    Debug_istream(const std::string& name, streams::Istream& stream):
        _name(name), _source(stream) {}
    type_safe::size_t _read(gsl::span<gsl::byte> s) override
    {
        auto bytes_read = _source.read(s);
        if (_enabled) {
            streams::print(streams::stderrs, "{}: _read({})\n",
                    _name, encode(s));
        }
        return bytes_read;
    }
    void enable() { _enabled = true; }
    void disable() { _enabled = false; }
};

int main()
{
    //Straight to standard out:
    streams::print(streams::stdouts, "{:>20} ${:X}\n", 3.1415926, 255);

    //Using a string stream:
    {
        streams::String_ostream sos;
        streams::print(sos, "{0} {0} {0}", "La");
        streams::print(streams::stdouts, "{:=^80}\n", sos.string());
    }

    //Using a file output stream:
    {
        streams::File_ostream fos("out.txt");
        std::time_t t(std::time(nullptr));
        streams::print(fos, "The date and time are {:%Y-%b-%d %T}.\n",
                *std::localtime(&t));
    }

    //Using a output pipe and binary output:
    {
        streams::Pipe_ostream pos("/opt/local/bin/xxd");
        uint8_t u8 = 0x01;
        uint16_t u16 = 0x0203;
        uint32_t u32 = 0x04050607;
        pos.put_data(u8);
        pos.put_data(u16);
        pos.put_data(u32);
    }

    //Using a buffered output stream:
    {
        //Stdout is already buffered, but using it to test Buffered_ostream.
        //Small buffer size for testing purposes.
#if 1
        streams::Buffered_ostream bos(streams::stdouts, 10u);
#else
        Debug_ostream os2("inner", streams::stdouts);
        streams::Buffered_ostream os1(os2, 10);
        Debug_ostream bos("outer", os1);
#endif
        for (int i = 0; i < 100; ++i)
            streams::print(bos, "{}, ", i);
        streams::prints(bos, "\n");
    }

    //Using an input file stream:
    {
        streams::File_istream fis("out.txt");
        auto line = fis.getline();
        streams::print(streams::stdouts, "{}\n", line.value_or("No line."));
    }

    //Creating a transformation ostream:
    {
        struct Shout_ostream: public streams::Ostream {
            Ostream& _sink;
            Shout_ostream(Ostream& s): _sink(s) {}
            type_safe::size_t _write(gsl::span<const gsl::byte> before)
                override
            {
                std::vector<gsl::byte> after;
                after.reserve(before.size());
                std::transform(before.begin(), before.end(),
                        std::back_inserter(after),
                        [](gsl::byte b) {
                            return static_cast<gsl::byte>(
                                    ::toupper(static_cast<int>(b)));
                        });
                return _sink.write(after);
            }
            void _flush() override { _sink.flush(); }
        } shout(streams::stdouts);
        streams::prints(shout, "Hello, world!\n");
    }

    //Using an Unget_istream:
    {
        streams::File_istream fis("out.txt");
        streams::Unget_istream uis(fis);
        auto dash = gsl::as_bytes(gsl::span<const char>("DASH"));
        auto colon = gsl::as_bytes(gsl::span<const char>("COLON"));
        //When converting the strings to spans, the size counts the NULs.
        dash = dash.first(dash.size() - 1);
        colon = colon.first(dash.size() - 1);
        while (true) {
            auto c = uis.get_byte();
            if (!c) break;
            if (c.value() == gsl::byte('-')) {
                uis.unget(dash);
                continue;
            }
            if (c.value() == gsl::byte(':')) {
                uis.unget(colon);
                continue;
            }
            streams::stdouts.put_byte(c.value());
        }
    }

    //Using Buffered_istream:
    {
        //A File_istream (built on stdio) should be buffered itself,
        //but using for testing purposes.
        //Small buffer size for testing purposes.
#if 1
        streams::File_istream fis("examples.cpp");
        streams::Buffered_istream bis(fis, 10u);
#else
        streams::File_istream fis_("examples.cpp");
        Debug_istream fis("inner", fis_);
        streams::Buffered_istream bis_(fis, 10u);
        Debug_istream bis("outer", bis_);
        bis.disable();
#endif
        size_t total = 0;
        while (true) {
            auto line = bis.getline();
            if (!line) break;
            total += line.value().size();
            streams::print(streams::stdouts, "{}\n", line.value());
            if (total > 100) break;
        }
    }
}

