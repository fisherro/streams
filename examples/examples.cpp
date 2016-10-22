#include <ctime>
#include <regex>
#include <string>
#include <fmt/time.h>
#include <streams/ostream.hpp>
#include <streams/istream.hpp>

struct Student {
    std::string name;
    int         id;
    float       gpa;
};

void write_student_binary(streams::ostream& out, const Student& student)
{
    out.put(student.name.size());
    out.write(gsl::as_bytes(gsl::span<const char>(
                    student.name.data(), student.name.size())));
    out.put(student.id);
    out.put(student.gpa);
}

streams::optional<Student> read_student_binary(streams::istream& in)
{
    Student student;
    auto size = in.get<std::string::size_type>();
    if (!size) return streams::nullopt;
    if (*size <= 0) return streams::nullopt;
    student.name.resize(*size);
    auto result = in.read(gsl::as_writeable_bytes(gsl::span<char>(
                    &student.name[0], student.name.size())));
    if (result.size() <= 0) return streams::nullopt;
    if (!in.get(student.id)) return streams::nullopt;
    if (!in.get(student.gpa)) return streams::nullopt;
    return student;
}

void write_student_text(streams::ostream& out, const Student& student)
{
    streams::print(out, "\"{}\",{},{}\n",
            student.name, student.id, student.gpa);
}

streams::optional<Student> read_student_text(streams::istream& in)
{
    //What to do about "formatted input"?
    //
    //In my experience, formatted input was never as easy as the standard
    //iostreams pretended. It is about matching patterns and splitting up
    //the input into small chunks that might then be used with an ad hoc
    //istringstream and operator>>.
    //(Or, more nicely, via boost::lexical_cast.)
    //
    //More thought needed.
    
    auto line = streams::get_line(in);
    if (!line) return streams::nullopt;
    std::regex rx(R"|("([^"]+)",([0-9]+),([0-9.]+))|");
    std::smatch match;
    if (!std::regex_match(*line, match, rx)) {
        throw std::string("Format mismatch!");
    }
    Student student;
    student.name = match[1];
    student.id = std::stoi(match[2]);
    student.gpa = std::stof(match[3]);
    return student;
}

void print_student_header()
{
    streams::print(streams::stdouts, "{:<10} {:^4} {:>5}\n",
            "NAME", "ID", "GPA");
}

void print_student(const Student& student)
{
    streams::print(streams::stdouts, "{:<10} {:^4} {:>5.2f}\n",
            student.name, student.id, student.gpa);
}

int main()
{
    std::vector<Student> class_roll{
        { "Alice", 12, 3.9 }, { "Bob", 23, 3.0 }, { "Chris", 34, 3.2 }
    };

    //Unformatted I/O:
    {
        streams::vector_ostream out;
        std::for_each(class_roll.begin(), class_roll.end(),
                [&out](const auto& student) {
                    write_student_binary(out, student);
                });
        streams::span_istream in(out.vector());
        std::vector<Student> roll2;
        while (true) {
            auto student = read_student_binary(in);
            if (!student) break;
            roll2.push_back(*student);
        }
        print_student_header();
        std::for_each(roll2.begin(), roll2.end(), print_student);
    }

    //Formatted I/O:
    {
        streams::vector_ostream out;
        std::for_each(class_roll.begin(), class_roll.end(),
                [&out](const auto& student) {
                    write_student_text(out, student);
                });
        streams::span_istream in(out.vector());
        std::vector<Student> roll2;
        while (true) {
            auto student = read_student_text(in);
            if (!student) break;
            roll2.push_back(*student);
        }
        print_student_header();
        std::for_each(roll2.begin(), roll2.end(), print_student);
    }
    
    //Character-based filter ostream:
    {
        struct Shout_ostream: public streams::ostream {
            streams::ostream& _sink;
            Shout_ostream(ostream& s): _sink(s) {}
            std::ptrdiff_t _write(gsl::span<const gsl::byte> before) override
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
        };

        Shout_ostream out(streams::stdouts);
        streams::put_line(out, "This is a test. This is only a test.");
    }

    //Line-based filter ostream:
    {
        struct Line_number_ostream: public streams::ostream {
            int _line = 0;
            streams::ostream& _sink;
            explicit Line_number_ostream(streams::ostream& out): _sink(out) {}
            std::ptrdiff_t _write(gsl::span<const gsl::byte> data) override
            {
                std::ptrdiff_t written = 0;
                if (0 == _line) {
                    //If this is the first time we're called,
                    //write the header for the first line.
                    streams::vector_ostream vos;
                    streams::print(vos, "{}: ", ++_line);
                    written += _sink.write(vos.vector());
                }
                do {
                    auto nl = std::find(data.begin(), data.end(),
                            gsl::byte('\n'));
                    if (data.end() == nl) {
                        //If there is now newline,
                        //write the data and we're done.
                        written += _sink.write(data);
                        break;
                    }
                    //Otherwise, write up to and including the newline...
                    auto count = std::distance(data.begin(), nl + 1);
                    written += _sink.write(data.first(count));
                    //...write the line header...
                    streams::vector_ostream vos;
                    streams::print(vos, "{}: ", ++_line);
                    written += _sink.write(vos.vector());
                    //...and most to past the newline.
                    data = data.subspan(count);
                } while (data.size() > 0);
                return written;
            }
            void _flush() override { _sink.flush(); }
        };

        Line_number_ostream lnos(streams::stdouts);
        streams::put_string(lnos,
                "Roses are red,\n"
                "Violets are blue,\n"
                "This poem has bugs,\n"
                "And...NO CARRIER");
        streams::put_char(streams::stdouts, '\n');
    }

    //Line-based filter otream, take 2:
    {
        struct Reverse_line_ostream: public streams::ostream {
            streams::ostream& _sink;
            std::vector<gsl::byte> _buffer;

            Reverse_line_ostream(ostream& s): _sink(s) {}

            std::ptrdiff_t _write(gsl::span<const gsl::byte> data) override
            {
                auto written = data.size();
                while (data.size() > 0) {
                    auto nl = std::find(data.begin(), data.end(),
                            gsl::byte('\n'));
                    if (data.end() != nl) {
                        auto count = std::distance(data.begin(), nl);
                        std::copy_n(data.begin(), count,
                                std::back_inserter(_buffer));
                        std::reverse(_buffer.begin(), _buffer.end());
                        _buffer.push_back(gsl::byte('\n'));
                        _sink.write(_buffer);
                        _sink.flush();
                        _buffer.clear();
                        data = data.subspan(count + 1);
                    } else {
                        std::copy(data.begin(), data.end(),
                                std::back_inserter(_buffer));
                        break;
                    }
                }
                return written;
            }

            ~Reverse_line_ostream()
            {
                try {
                    if (!_buffer.empty()) {
                        std::reverse(_buffer.begin(), _buffer.end());
                        _sink.write(_buffer);
                        _sink.flush();
                    }
                } catch (...) {
                    //Don't let exceptions escape dtors!
                }
            }
        };

        {
            Reverse_line_ostream rlos(streams::stdouts);
            streams::put_string(rlos,
                    "Roses are red,\n"
                    "Violets are blue,\n"
                    "This poem has bugs,\n"
                    "And...NO CARRIER");
        }
        streams::put_char(streams::stdouts, '\n');
    }
}

