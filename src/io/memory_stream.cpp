#include <tracker/io/memory_stream.hpp>
#include <cstring>
#include <algorithm>

namespace tracker::io {

MemoryInputStream::MemoryInputStream(const uint8_t* data, size_t size)
    : m_data(data), m_size(size), m_pos(0) {}

MemoryInputStream::MemoryInputStream(const std::vector<uint8_t>& vec)
    : m_data(vec.data()), m_size(vec.size()), m_pos(0) {}

size_t MemoryInputStream::read(void* dest, size_t num_bytes) {
    if (!m_data || m_pos >= m_size || num_bytes == 0) return 0;
    size_t available = m_size - m_pos;
    size_t to_read = std::min(num_bytes, available);
    std::memcpy(dest, m_data + m_pos, to_read);
    m_pos += to_read;
    return to_read;
}

bool MemoryInputStream::seek(int64_t offset, SeekOrigin origin) {
    int64_t new_pos = static_cast<int64_t>(m_pos);
    if (origin == SeekOrigin::Begin) new_pos = offset;
    else if (origin == SeekOrigin::Current) new_pos += offset;
    else if (origin == SeekOrigin::End) new_pos = static_cast<int64_t>(m_size) + offset;

    if (new_pos < 0 || new_pos > static_cast<int64_t>(m_size)) return false;
    m_pos = static_cast<size_t>(new_pos);
    return true;
}

int64_t MemoryInputStream::tell() const { return static_cast<int64_t>(m_pos); }
int64_t MemoryInputStream::size() const { return static_cast<int64_t>(m_size); }
bool MemoryInputStream::eof() const { return m_pos >= m_size; }

MemoryOutputStream::MemoryOutputStream(size_t reserve_capacity) {
    m_buffer.reserve(reserve_capacity);
}

size_t MemoryOutputStream::write(const void* src, size_t num_bytes) {
    if (!src || num_bytes == 0) return 0;
    if (m_pos + num_bytes > m_buffer.size()) {
        m_buffer.resize(m_pos + num_bytes);
    }
    std::memcpy(m_buffer.data() + m_pos, src, num_bytes);
    m_pos += num_bytes;
    return num_bytes;
}

bool MemoryOutputStream::seek(int64_t offset, SeekOrigin origin) {
    int64_t new_pos = static_cast<int64_t>(m_pos);
    if (origin == SeekOrigin::Begin) new_pos = offset;
    else if (origin == SeekOrigin::Current) new_pos += offset;
    else if (origin == SeekOrigin::End) new_pos = static_cast<int64_t>(m_buffer.size()) + offset;

    if (new_pos < 0 || new_pos > static_cast<int64_t>(m_buffer.size())) return false;
    m_pos = static_cast<size_t>(new_pos);
    return true;
}

int64_t MemoryOutputStream::tell() const { return static_cast<int64_t>(m_pos); }

} // namespace tracker::io
