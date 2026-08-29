#include <tracker/io/file_stream.hpp>

namespace tracker::io {

FileInputStream::FileInputStream(std::FILE* fp, int64_t file_size)
    : m_fp(fp), m_size(file_size) {}

FileInputStream::~FileInputStream() {
    if (m_fp) {
        std::fclose(m_fp);
        m_fp = nullptr;
    }
}

FileInputStream::FileInputStream(FileInputStream&& other) noexcept
    : m_fp(other.m_fp), m_size(other.m_size) {
    other.m_fp = nullptr;
    other.m_size = 0;
}

FileInputStream& FileInputStream::operator=(FileInputStream&& other) noexcept {
    if (this != &other) {
        if (m_fp) std::fclose(m_fp);
        m_fp = other.m_fp;
        m_size = other.m_size;
        other.m_fp = nullptr;
        other.m_size = 0;
    }
    return *this;
}

Result<FileInputStream> FileInputStream::open(const std::string& path) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        return Result<FileInputStream>(ErrorCode::IoError, "Failed to open file for reading: " + path);
    }
    std::fseek(fp, 0, SEEK_END);
    int64_t sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    return Result<FileInputStream>(FileInputStream(fp, sz));
}

size_t FileInputStream::read(void* dest, size_t num_bytes) {
    if (!m_fp || num_bytes == 0) return 0;
    return std::fread(dest, 1, num_bytes, m_fp);
}

bool FileInputStream::seek(int64_t offset, SeekOrigin origin) {
    if (!m_fp) return false;
    int c_origin = SEEK_SET;
    if (origin == SeekOrigin::Current) c_origin = SEEK_CUR;
    else if (origin == SeekOrigin::End) c_origin = SEEK_END;
    return std::fseek(m_fp, static_cast<long>(offset), c_origin) == 0;
}

int64_t FileInputStream::tell() const {
    if (!m_fp) return 0;
    return std::ftell(m_fp);
}

int64_t FileInputStream::size() const { return m_size; }

bool FileInputStream::eof() const {
    if (!m_fp) return true;
    return std::feof(m_fp) != 0 || tell() >= m_size;
}

FileOutputStream::FileOutputStream(std::FILE* fp) : m_fp(fp) {}

FileOutputStream::~FileOutputStream() {
    if (m_fp) {
        std::fclose(m_fp);
        m_fp = nullptr;
    }
}

FileOutputStream::FileOutputStream(FileOutputStream&& other) noexcept : m_fp(other.m_fp) {
    other.m_fp = nullptr;
}

FileOutputStream& FileOutputStream::operator=(FileOutputStream&& other) noexcept {
    if (this != &other) {
        if (m_fp) std::fclose(m_fp);
        m_fp = other.m_fp;
        other.m_fp = nullptr;
    }
    return *this;
}

Result<FileOutputStream> FileOutputStream::open(const std::string& path) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        return Result<FileOutputStream>(ErrorCode::IoError, "Failed to open file for writing: " + path);
    }
    return Result<FileOutputStream>(FileOutputStream(fp));
}

size_t FileOutputStream::write(const void* src, size_t num_bytes) {
    if (!m_fp || num_bytes == 0) return 0;
    return std::fwrite(src, 1, num_bytes, m_fp);
}

bool FileOutputStream::seek(int64_t offset, SeekOrigin origin) {
    if (!m_fp) return false;
    int c_origin = SEEK_SET;
    if (origin == SeekOrigin::Current) c_origin = SEEK_CUR;
    else if (origin == SeekOrigin::End) c_origin = SEEK_END;
    return std::fseek(m_fp, static_cast<long>(offset), c_origin) == 0;
}

int64_t FileOutputStream::tell() const {
    if (!m_fp) return 0;
    return std::ftell(m_fp);
}

} // namespace tracker::io
