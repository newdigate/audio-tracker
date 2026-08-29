#pragma once
#include <tracker/io/stream.hpp>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace tracker::io {

class MemoryInputStream : public InputStream {
public:
    MemoryInputStream(const uint8_t* data, size_t size);
    explicit MemoryInputStream(const std::vector<uint8_t>& vec);

    size_t read(void* dest, size_t num_bytes) override;
    bool seek(int64_t offset, SeekOrigin origin) override;
    int64_t tell() const override;
    int64_t size() const override;
    bool eof() const override;

private:
    const uint8_t* m_data{nullptr};
    size_t m_size{0};
    size_t m_pos{0};
};

class MemoryOutputStream : public OutputStream {
public:
    MemoryOutputStream() = default;
    explicit MemoryOutputStream(size_t reserve_capacity);

    size_t write(const void* src, size_t num_bytes) override;
    bool seek(int64_t offset, SeekOrigin origin) override;
    int64_t tell() const override;

    const std::vector<uint8_t>& data() const noexcept { return m_buffer; }
    std::vector<uint8_t>& data() noexcept { return m_buffer; }
    std::vector<uint8_t> take_data() noexcept { return std::move(m_buffer); }

private:
    std::vector<uint8_t> m_buffer;
    size_t m_pos{0};
};

} // namespace tracker::io
