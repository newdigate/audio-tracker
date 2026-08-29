#include "test_main.hpp"
#include <tracker/io/memory_stream.hpp>

TEST_CASE(MemoryStream_ReadWrite) {
    tracker::io::MemoryOutputStream out;
    out.write_u32_le(0x12345678);
    out.write_fixed_string("HELLO", 8, '.');

    const auto& data = out.data();
    REQUIRE_EQ(data.size(), 12);

    tracker::io::MemoryInputStream in(data.data(), data.size());
    REQUIRE_EQ(in.size(), 12);
    REQUIRE_EQ(in.read_u32_le(), 0x12345678);
    REQUIRE_EQ(in.read_fixed_string(8), "HELLO...");
    REQUIRE(in.eof());
}

TEST_CASE(MemoryStream_SeekAndTell) {
    tracker::io::MemoryOutputStream out;
    out.write_u32_le(0x11223344);
    out.write_u32_le(0x55667788);
    REQUIRE_EQ(out.tell(), 8);

    REQUIRE(out.seek(0, tracker::io::SeekOrigin::Begin));
    REQUIRE_EQ(out.tell(), 0);
    out.write_u8(0xFF);
    REQUIRE_EQ(out.tell(), 1);

    auto vec = out.take_data();
    REQUIRE_EQ(vec.size(), 8);
    REQUIRE_EQ(vec[0], 0xFF);

    tracker::io::MemoryInputStream in(vec);
    REQUIRE_EQ(in.tell(), 0);
    REQUIRE_EQ(in.size(), 8);
    REQUIRE(!in.eof());

    REQUIRE(in.seek(4, tracker::io::SeekOrigin::Begin));
    REQUIRE_EQ(in.tell(), 4);
    REQUIRE_EQ(in.read_u32_le(), 0x55667788);
    REQUIRE(in.eof());

    REQUIRE(in.seek(-4, tracker::io::SeekOrigin::Current));
    REQUIRE_EQ(in.tell(), 4);
    REQUIRE(in.seek(-8, tracker::io::SeekOrigin::End));
    REQUIRE_EQ(in.tell(), 0);

    // Invalid seeks
    REQUIRE(!in.seek(-1, tracker::io::SeekOrigin::Begin));
    REQUIRE(!in.seek(9, tracker::io::SeekOrigin::Begin));
}

TEST_CASE(MemoryStream_EdgeCases) {
    tracker::io::MemoryOutputStream out(64);
    REQUIRE_EQ(out.tell(), 0);
    REQUIRE_EQ(out.data().size(), 0);

    // Seek in empty stream
    REQUIRE(out.seek(0, tracker::io::SeekOrigin::Begin));
    REQUIRE(!out.seek(1, tracker::io::SeekOrigin::Begin));
    REQUIRE(!out.seek(-1, tracker::io::SeekOrigin::Begin));

    out.write_u8(0xAA);
    out.write_u8(0xBB);
    REQUIRE_EQ(out.tell(), 2);
    REQUIRE(out.seek(-1, tracker::io::SeekOrigin::End));
    REQUIRE_EQ(out.tell(), 1);

    tracker::io::MemoryInputStream in(nullptr, 0);
    REQUIRE_EQ(in.size(), 0);
    REQUIRE_EQ(in.tell(), 0);
    REQUIRE(in.eof());
    uint8_t dummy = 0;
    REQUIRE_EQ(in.read(&dummy, 1), 0);
    REQUIRE_EQ(in.read(nullptr, 10), 0);
}
