#include <tracker/io/stream.hpp>
#include <vector>

namespace tracker::io {

uint8_t InputStream::read_u8() {
    uint8_t val = 0;
    read(&val, 1);
    return val;
}

int8_t InputStream::read_i8() {
    int8_t val = 0;
    read(&val, 1);
    return val;
}

uint16_t InputStream::read_u16_le() {
    uint8_t b[2] = {0, 0};
    read(b, 2);
    return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

int16_t InputStream::read_i16_le() {
    return static_cast<int16_t>(read_u16_le());
}

uint32_t InputStream::read_u32_le() {
    uint8_t b[4] = {0, 0, 0, 0};
    read(b, 4);
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t InputStream::read_u16_be() {
    uint8_t b[2] = {0, 0};
    read(b, 2);
    return (static_cast<uint16_t>(b[0]) << 8) | static_cast<uint16_t>(b[1]);
}

int16_t InputStream::read_i16_be() {
    return static_cast<int16_t>(read_u16_be());
}

uint32_t InputStream::read_u32_be() {
    uint8_t b[4] = {0, 0, 0, 0};
    read(b, 4);
    return (static_cast<uint32_t>(b[0]) << 24) |
           (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) |
           static_cast<uint32_t>(b[3]);
}

std::string InputStream::read_fixed_string(size_t len) {
    if (len == 0) return {};
    std::string str(len, '\0');
    size_t bytes_read = read(str.data(), len);
    str.resize(bytes_read);
    return str;
}

bool InputStream::skip(size_t num_bytes) {
    return seek(static_cast<int64_t>(num_bytes), SeekOrigin::Current);
}

void OutputStream::write_u8(uint8_t val) {
    write(&val, 1);
}

void OutputStream::write_i8(int8_t val) {
    write(&val, 1);
}

void OutputStream::write_u16_le(uint16_t val) {
    uint8_t b[2] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF)
    };
    write(b, 2);
}

void OutputStream::write_i16_le(int16_t val) {
    write_u16_le(static_cast<uint16_t>(val));
}

void OutputStream::write_u32_le(uint32_t val) {
    uint8_t b[4] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 24) & 0xFF)
    };
    write(b, 4);
}

void OutputStream::write_u16_be(uint16_t val) {
    uint8_t b[2] = {
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>(val & 0xFF)
    };
    write(b, 2);
}

void OutputStream::write_i16_be(int16_t val) {
    write_u16_be(static_cast<uint16_t>(val));
}

void OutputStream::write_u32_be(uint32_t val) {
    uint8_t b[4] = {
        static_cast<uint8_t>((val >> 24) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>(val & 0xFF)
    };
    write(b, 4);
}

void OutputStream::write_fixed_string(const std::string& str, size_t fixed_len, char pad) {
    for (size_t i = 0; i < fixed_len; ++i) {
        uint8_t ch = (i < str.size()) ? static_cast<uint8_t>(str[i]) : static_cast<uint8_t>(pad);
        write_u8(ch);
    }
}

void OutputStream::write_zeros(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        write_u8(0);
    }
}

} // namespace tracker::io
