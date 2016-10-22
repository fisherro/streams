#define CATCH_CONFIG_MAIN
#include <cstdint>
#include <ctime>
#include <vector>
#include <catch.hpp>
#include <gsl/gsl>
#include <fmt/time.h>
#include "streams/ostream.hpp"
#include "streams/istream.hpp"

namespace {
    template<typename T>
    gsl::span<const gsl::byte> to_byte_span(const T& t)
    { return gsl::as_bytes(gsl::span<const T>(&t, 1)); }
}

TEST_CASE("streams", "[streams]")
{
    const std::vector<char> cv{
        0x01,
            0x02, 0x02,
            0x03, 0x03, 0x03, 0x03,
            0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
    };
    std::vector<gsl::byte> control;
    for (auto c: cv) control.push_back(gsl::byte(c));

    SECTION("span_ostream") {
        std::vector<gsl::byte> data;
        data.resize(control.size());
        streams::span_ostream stream(data);
        stream.put<std::int8_t>(0x01);
        stream.put<std::int16_t>(0x0202);
        stream.put<std::int32_t>(0x03030303);
        REQUIRE(stream.unused().size() == sizeof(std::int64_t));
        stream.put<std::int64_t>(0x0404040404040404);
        REQUIRE(control == data);
    }

    SECTION("vector_ostream") {
        streams::vector_ostream stream;
        stream.put<std::int8_t>(0x01);
        stream.put<std::int16_t>(0x0202);
        stream.put<std::int32_t>(0x03030303);
        stream.put<std::int64_t>(0x0404040404040404);
        const auto& data = stream.vector();
        REQUIRE(control == data);
    }

    SECTION("buf_ostream") {
        streams::vector_ostream vos;
        {
            streams::buf_ostream stream(vos, 10);
            stream.write(control);
        }
        const auto& data = vos.vector();
        REQUIRE(control == data);
    }

    //streams::print
    SECTION("print") {
        std::vector<gsl::byte> control;
        for (auto c: "255;ff;377;11111111") control.push_back(gsl::byte(c));
        //Remove the NUL terminator from the vector.
        control.pop_back();
        streams::vector_ostream stream;
        streams::print(stream, "{0:d};{0:x};{0:o};{0:b}", 0x00FF);
        auto& data = stream.vector();
        REQUIRE(control == data);
    }
    
    //streams::put_string
    SECTION("print") {
        std::vector<gsl::byte> control;
        for (auto c: "255;ff;377;11111111") control.push_back(gsl::byte(c));
        //Remove the NUL terminator from the vector.
        control.pop_back();
        streams::vector_ostream stream;
        streams::put_string(stream, "255;ff;377;11111111");
        auto& data = stream.vector();
        REQUIRE(control == data);
    }

    //streams::put_line
    SECTION("print") {
        std::vector<gsl::byte> control;
        for (auto c: "255;ff;377;11111111\n") control.push_back(gsl::byte(c));
        //Remove the NUL terminator from the vector.
        control.pop_back();
        streams::vector_ostream stream;
        streams::put_line(stream, "255;ff;377;11111111");
        auto& data = stream.vector();
        REQUIRE(control == data);
    }

    //streams::put_char
    SECTION("put_char") {
        streams::vector_ostream stream;
        streams::put_char(stream, 'A');
        REQUIRE(gsl::byte('A') == stream.vector()[0]);
    }
    
    SECTION("span_istream") {
        streams::span_istream stream(
                gsl::span<const gsl::byte>(control.data(), control.size()));
        auto n8 = stream.get<std::int8_t>();
        auto n16 = stream.get<std::int16_t>();
        auto n32 = stream.get<std::int32_t>();
        auto n64 = stream.get<std::int64_t>();
        REQUIRE(*n8 == 0x01);
        REQUIRE(*n16 == 0x0202);
        REQUIRE(*n32 == 0x03030303);
        REQUIRE(*n64 == 0x0404040404040404);
    }

    SECTION("buf_istream") {
        streams::span_istream sis(
                gsl::span<const gsl::byte>(control.data(), control.size()));
        streams::buf_istream stream(sis, 3);
        auto n8 = stream.get<std::int8_t>();
        auto n16 = stream.get<std::int16_t>();
        auto n32 = stream.get<std::int32_t>();
        auto n64 = stream.get<std::int64_t>();
        REQUIRE(*n8 == 0x01);
        REQUIRE(*n16 == 0x0202);
        REQUIRE(*n32 == 0x03030303);
        REQUIRE(*n64 == 0x0404040404040404);
    }

    SECTION("unget_istream") {
        streams::span_istream sis(
                gsl::span<const gsl::byte>(control.data(), control.size()));
        streams::unget_istream stream(sis);
        auto n8 = stream.get<std::int8_t>();
        auto n16 = stream.get<std::int16_t>();
        auto n32 = stream.get<std::int32_t>();
        REQUIRE(*n8 == 0x01);
        REQUIRE(*n16 == 0x0202);
        REQUIRE(*n32 == 0x03030303);
        stream.unget(to_byte_span(int8_t(0x05)));
        stream.unget(to_byte_span(int16_t(0x0606)));
        stream.unget(to_byte_span(int32_t(0x07070707)));
        n32 = stream.get<std::int32_t>();
        n16 = stream.get<std::int16_t>();
        n8 = stream.get<std::int8_t>();
        auto n64 = stream.get<std::int64_t>();
        REQUIRE(*n8 == 0x05);
        REQUIRE(*n16 == 0x0606);
        REQUIRE(*n32 == 0x07070707);
        REQUIRE(*n64 == 0x0404040404040404);
    }

    SECTION("get_line") {
        std::string control = "This is a test.\nThis is only a test.";
        streams::span_istream stream(gsl::as_bytes(
                    gsl::span<const char>(control.data(), control.size())));
        auto line1 = streams::get_line(stream);
        auto line2 = streams::get_line(stream);
        REQUIRE(*line1 == "This is a test.");
        REQUIRE(*line2 == "This is only a test.");
    }

    SECTION("get_char") {
        std::string control = "This is a test.\nThis is only a test.";
        streams::span_istream stream(gsl::as_bytes(
                    gsl::span<const char>(control.data(), control.size())));
        auto c = streams::get_char(stream);
        REQUIRE(*c == 'T');
    }

    SECTION("stdio_file_*stream") {
        const std::string fname("test.txt");
        std::time_t t(std::time(nullptr));
        std::string date = fmt::format("{:%Y-%b-%d %T}", *std::localtime(&t));
        {
            streams::stdio_file_ostream out(fname);
            streams::put_string(out, date);
        }
        {
            streams::stdio_file_istream in(fname);
            auto line = streams::get_line(in);
            REQUIRE(*line == date);
        }
    }

    //streams::stdio_pipe_*stream
    SECTION("stdio_pipe_*stream") {
        const std::string fname("base64.txt");
        std::time_t t(std::time(nullptr));
        std::string date = fmt::format("{:%Y-%b-%d %T}", *std::localtime(&t));
        {
            std::string command = fmt::format("base64 -o {}", fname);
            streams::stdio_pipe_ostream out(command);
            streams::put_string(out, date);
        }
        {
            std::string command = fmt::format("base64 -D -i {}", fname);
            streams::stdio_pipe_istream in(command);
            auto line = streams::get_line(in);
            REQUIRE(*line == date);
        }
    }
}
