#include "test_main.hpp"
#include <tracker/io/stream.hpp>
#include <vector>
#include <cstring>

class DummyStream : public tracker::io::InputStream, public tracker::io::OutputStream {
public:
    std::vector<uint8_t> buffer;
    int64_t pos{0};

    size_t read(void* dest, size_t num_bytes) override {
        if (pos >= static_cast<int64_t>(buffer.size())) return 0;
        size_t available = buffer.size() - static_cast<size_t>(pos);
        size_t to_read = (num_bytes < available) ? num_bytes : available;
        std::memcpy(dest, buffer.data() + pos, to_read);
        pos += to_read;
        return to_read;
    }

    size_t write(const void* src, size_t num_bytes) override {
        if (pos + num_bytes > buffer.size()) {
            buffer.resize(pos + num_bytes);
        }
        std::memcpy(buffer.data() + pos, src, num_bytes);
        pos += num_bytes;
        return num_bytes;
    }

    bool seek(int64_t offset, tracker::io::SeekOrigin origin) override {
        int64_t new_pos = pos;
        if (origin == tracker::io::SeekOrigin::Begin) new_pos = offset;
        else if (origin == tracker::io::SeekOrigin::Current) new_pos += offset;
        else if (origin == tracker::io::SeekOrigin::End) new_pos = static_cast<int64_t>(buffer.size()) + offset;
        if (new_pos < 0 || new_pos > static_cast<int64_t>(buffer.size())) return false;
        pos = new_pos;
        return true;
    }

    int64_t tell() const override { return pos; }
    int64_t size() const override { return static_cast<int64_t>(buffer.size()); }
    bool eof() const override { return pos >= static_cast<int64_t>(buffer.size()); }
};

TEST_CASE(Stream_EndianHelpers) {
    DummyStream s;
    s.write_u8(0x42);
    s.write_i8(-5);
    s.write_u16_le(0x1234);
    s.write_i16_le(-1000);
    s.write_u32_le(0xdeadbeef);
    s.write_fixed_string("FT2", 6, ' ');

    REQUIRE_EQ(s.size(), 1 + 1 + 2 + 2 + 4 + 6);

    s.seek(0, tracker::io::SeekOrigin::Begin);
    REQUIRE_EQ(s.read_u8(), 0x42);
    REQUIRE_EQ(s.read_i8(), -5);
    REQUIRE_EQ(s.read_u16_le(), 0x1234);
    REQUIRE_EQ(s.read_i16_le(), -1000);
    REQUIRE_EQ(s.read_u32_le(), 0xdeadbeef);
    REQUIRE_EQ(s.read_fixed_string(6), "FT2   ");
}
