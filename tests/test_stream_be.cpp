#include "test_main.hpp"
#include <tracker/io/memory_stream.hpp>

TEST_CASE(Stream_BigEndianHelpers) {
    tracker::io::MemoryOutputStream out;
    out.write_u16_be(0x1234);
    out.write_i16_be(-1000);
    out.write_u32_be(0xdeadbeef);

    const auto& data = out.data();
    REQUIRE_EQ(data.size(), 2 + 2 + 4);

    // Verify raw big-endian byte layout
    REQUIRE_EQ(data[0], 0x12);
    REQUIRE_EQ(data[1], 0x34);
    REQUIRE_EQ(data[4], 0xde);
    REQUIRE_EQ(data[5], 0xad);
    REQUIRE_EQ(data[6], 0xbe);
    REQUIRE_EQ(data[7], 0xef);

    tracker::io::MemoryInputStream in(data.data(), data.size());
    REQUIRE_EQ(in.read_u16_be(), 0x1234);
    REQUIRE_EQ(in.read_i16_be(), -1000);
    REQUIRE_EQ(in.read_u32_be(), 0xdeadbeef);
}
