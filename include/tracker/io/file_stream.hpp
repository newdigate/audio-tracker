#pragma once
#include <tracker/io/stream.hpp>
#include <tracker/types.hpp>
#include <cstdio>
#include <string>

namespace tracker::io {

class FileInputStream : public InputStream {
public:
    FileInputStream() = default;
    ~FileInputStream() override;
    static Result<FileInputStream> open(const std::string& path);

    FileInputStream(FileInputStream&& other) noexcept;
    FileInputStream& operator=(FileInputStream&& other) noexcept;
    FileInputStream(const FileInputStream&) = delete;
    FileInputStream& operator=(const FileInputStream&) = delete;

    size_t read(void* dest, size_t num_bytes) override;
    bool seek(int64_t offset, SeekOrigin origin) override;
    int64_t tell() const override;
    int64_t size() const override;
    bool eof() const override;

private:
    explicit FileInputStream(std::FILE* fp, int64_t file_size);
    std::FILE* m_fp{nullptr};
    int64_t m_size{0};
};

class FileOutputStream : public OutputStream {
public:
    FileOutputStream() = default;
    ~FileOutputStream() override;
    static Result<FileOutputStream> open(const std::string& path);

    FileOutputStream(FileOutputStream&& other) noexcept;
    FileOutputStream& operator=(FileOutputStream&& other) noexcept;
    FileOutputStream(const FileOutputStream&) = delete;
    FileOutputStream& operator=(const FileOutputStream&) = delete;

    size_t write(const void* src, size_t num_bytes) override;
    bool seek(int64_t offset, SeekOrigin origin) override;
    int64_t tell() const override;

private:
    explicit FileOutputStream(std::FILE* fp);
    std::FILE* m_fp{nullptr};
};

} // namespace tracker::io
