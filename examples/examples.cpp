#include <ctime>
#include <fmt/time.h>
#include <streams/ostream.hpp>
#include <streams/istream.hpp>

int main()
{
    //Straight to standard out:
    streams::print(streams::stdouts, "{:>20} ${:X}\n", 3.1415926, 255);

    {
        //Using a string stream:
        streams::String_ostream sos;
        streams::print(sos, "{0} {0} {0}", "La");
        streams::print(streams::stdouts, "{:=^80}\n", sos.string());
    }

    {
        //Using a file output stream:
        streams::File_ostream fos("out.txt");
        std::time_t t(std::time(nullptr));
        streams::print(fos, "The date and time are {:%Y-%b-%d %T}.\n",
                *std::localtime(&t));
    }

    {
        //Using a output pipe and binary output:
        streams::Pipe_ostream pos("/opt/local/bin/xxd");
        uint8_t u8 = 0x01;
        uint16_t u16 = 0x0203;
        uint32_t u32 = 0x04050607;
        pos.put_data(u8);
        pos.put_data(u16);
        pos.put_data(u32);
    }

    {
        //Using a buffered output stream:
        streams::Buffered_ostream bos(streams::stdouts, 10);
        for (int i = 0; i < 100; ++i)
            streams::print(bos, "{}, ", i);
        streams::prints(bos, "\n");
        bos.flush();//TODO: This shouldn't be necessary.
    }

    {
        //Using an input file stream:
        streams::File_istream fis("out.txt");
        auto line = fis.getline();
        if (line) {
            streams::print(streams::stdouts, "{}\n", *line);
        } else {
            streams::prints(streams::stdouts, "No line.\n");
        }
    }

    {
        //Creating a transformation ostream:
        struct Shout_ostream: public streams::Ostream {
            Ostream& _stream;
            Shout_ostream(Ostream& s): _stream(s) {}
            size_t _write(gsl::span<const gsl::byte> before)
            {
                std::vector<gsl::byte> after;
                after.reserve(before.size());
                std::transform(before.begin(), before.end(),
                        std::back_inserter(after),
                        [](gsl::byte b) {
                            return static_cast<gsl::byte>(
                                    ::toupper(static_cast<int>(b)));
                        });
                return _stream.write(after);
            }
        } shout(streams::stdouts);
        streams::prints(shout, "Hello, world!");
    }
}
