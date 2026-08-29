#include "test_main.hpp"
#include <tracker/io/file_stream.hpp>
#include <cstdio>

TEST_CASE(FileStream_ReadWrite) {
    const std::string path = "test_temp_stream.bin";
    {
        auto out_res = tracker::io::FileOutputStream::open(path);
        REQUIRE(out_res.is_ok());
        auto& out = out_res.value();
        out.write_u32_le(0xCAFEBABE);
        out.write_fixed_string("FILEIO", 10, ' ');
    }

    {
        auto in_res = tracker::io::FileInputStream::open(path);
        REQUIRE(in_res.is_ok());
        auto& in = in_res.value();
        REQUIRE_EQ(in.size(), 14);
        REQUIRE_EQ(in.read_u32_le(), 0xCAFEBABE);
        REQUIRE_EQ(in.read_fixed_string(10), "FILEIO    ");
    }

    std::remove(path.c_str());
}

TEST_CASE(FileStream_NotFound) {
    auto in_res = tracker::io::FileInputStream::open("non_existent_file_tracker_test.bin");
    REQUIRE(!in_res.is_ok());
    REQUIRE_EQ(in_res.status().code, tracker::ErrorCode::IoError);
}

TEST_CASE(FileStream_SeekAndTell) {
    const std::string path = "test_temp_seek.bin";
    {
        auto out_res = tracker::io::FileOutputStream::open(path);
        REQUIRE(out_res.is_ok());
        auto& out = out_res.value();
        out.write_u32_le(0x11223344);
        out.write_u32_le(0x55667788);
        REQUIRE_EQ(out.tell(), 8);
    }

    {
        auto in_res = tracker::io::FileInputStream::open(path);
        REQUIRE(in_res.is_ok());
        auto& in = in_res.value();
        REQUIRE_EQ(in.size(), 8);
        REQUIRE_EQ(in.tell(), 0);

        REQUIRE(in.seek(4, tracker::io::SeekOrigin::Begin));
        REQUIRE_EQ(in.tell(), 4);
        REQUIRE_EQ(in.read_u32_le(), 0x55667788);
        REQUIRE(in.eof());

        REQUIRE(in.seek(0, tracker::io::SeekOrigin::Begin));
        REQUIRE_EQ(in.tell(), 0);
        REQUIRE_EQ(in.read_u32_le(), 0x11223344);
    }

    std::remove(path.c_str());
}

TEST_CASE(FileStream_MoveSemantics) {
    const std::string path = "test_temp_move.bin";
    {
        auto out_res = tracker::io::FileOutputStream::open(path);
        REQUIRE(out_res.is_ok());
        tracker::io::FileOutputStream out = std::move(out_res.value());
        out.write_u32_le(0x99887766);
    }

    {
        auto in_res = tracker::io::FileInputStream::open(path);
        REQUIRE(in_res.is_ok());
        tracker::io::FileInputStream in = std::move(in_res.value());
        REQUIRE_EQ(in.size(), 4);
        REQUIRE_EQ(in.read_u32_le(), 0x99887766);
        REQUIRE(in.eof());
    }

    std::remove(path.c_str());
}

TEST_CASE(FileStream_WriteError) {
    auto out_res = tracker::io::FileOutputStream::open("/non_existent_dir_999999/test.bin");
    REQUIRE(!out_res.is_ok());
    REQUIRE_EQ(out_res.status().code, tracker::ErrorCode::IoError);
}
