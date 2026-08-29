#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace tracker::io {

enum class SeekOrigin {
    Begin,
    Current,
    End
};

class InputStream {
public:
    virtual ~InputStream() = default;

    virtual size_t read(void* dest, size_t num_bytes) = 0;
    virtual bool seek(int64_t offset, SeekOrigin origin) = 0;
    virtual int64_t tell() const = 0;
    virtual int64_t size() const = 0;
    virtual bool eof() const = 0;

    uint8_t  read_u8();
    int8_t   read_i8();
    uint16_t read_u16_le();
    int16_t  read_i16_le();
    uint32_t read_u32_le();
    uint16_t read_u16_be();
    int16_t  read_i16_be();
    uint32_t read_u32_be();
    std::string read_fixed_string(size_t len);
    bool skip(size_t num_bytes);
};

class OutputStream {
public:
    virtual ~OutputStream() = default;

    virtual size_t write(const void* src, size_t num_bytes) = 0;
    virtual bool seek(int64_t offset, SeekOrigin origin) = 0;
    virtual int64_t tell() const = 0;

    void write_u8(uint8_t val);
    void write_i8(int8_t val);
    void write_u16_le(uint16_t val);
    void write_i16_le(int16_t val);
    void write_u32_le(uint32_t val);
    void write_u16_be(uint16_t val);
    void write_i16_be(int16_t val);
    void write_u32_be(uint32_t val);
    void write_fixed_string(const std::string& str, size_t fixed_len, char pad = ' ');
    void write_zeros(size_t count);
};

} // namespace tracker::io
