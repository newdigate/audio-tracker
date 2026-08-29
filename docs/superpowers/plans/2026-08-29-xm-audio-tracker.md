# XM Audio Tracker Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a clean, portable C++17 library (`audio_tracker`) to read and write FastTracker II Extended Module (`.xm`) files with zero external dependencies and full compatibility with embedded microcontrollers (Teensy 4.x / NXP i.MX RT1176).

**Architecture:** The library consists of a format-agnostic tracker data model (`tracker::Song`, `Pattern`, `Cell`, `Instrument`, `Sample`), a lightweight Little-Endian I/O stream abstraction (`InputStream`, `OutputStream`, `MemoryInputStream`, `MemoryOutputStream`, `FileInputStream`, `FileOutputStream`), and dedicated `.xm` codecs (`XmReader`, `XmWriter`, `xm_delta`, `xm_pattern_codec`) using value-based error reporting (`Result<T>`, `Status`).

**Tech Stack:** C++17, CMake 3.16+, `-fno-exceptions`, `-fno-rtti`, Little-Endian binary I/O, custom zero-dependency unit test runner.

**Spec:** [`docs/superpowers/specs/2026-08-29-xm-audio-tracker-design.md`](file:///Users/moolet/Development/github/newdigate/audio-tracker/docs/superpowers/specs/2026-08-29-xm-audio-tracker-design.md)

## Global Constraints
* Language: C++17 (`cxx_std_17`).
* No external dependencies beyond the standard library (C++ STL containers).
* Zero C++ exceptions (`-fno-exceptions`) and zero RTTI (`-fno-rtti`) compatible.
* Little-Endian binary multi-byte integer serialization.
* Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

---

### Task 1: Core Types & Abstract Stream I/O

**Files:**
- Create: `include/tracker/types.hpp`
- Create: `include/tracker/io/stream.hpp`
- Create: `src/io/stream.cpp`
- Create: `tests/test_main.hpp`
- Create: `tests/test_types.cpp`
- Create: `tests/test_stream.cpp`
- Create: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::ErrorCode`, `tracker::Status`, `tracker::Result<T>`, `tracker::io::SeekOrigin`, `tracker::io::InputStream`, `tracker::io::OutputStream`.

- [ ] **Step 1: Write minimal test runner header and failing tests**

`tests/test_main.hpp`:
```cpp
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>

namespace tracker::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool register_test(const std::string& name, std::function<void()> func) {
    get_registry().push_back({name, std::move(func)});
    return true;
}

#define TEST_CASE(name) \
    static void test_func_##name(); \
    static const bool test_reg_##name = tracker::test::register_test(#name, test_func_##name); \
    static void test_func_##name()

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

#define REQUIRE_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "FAILED: " << #a << " == " << #b << " (got " << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

} // namespace tracker::test
```

`tests/test_types.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/types.hpp>

TEST_CASE(Types_StatusAndResult) {
    tracker::Status ok_status = tracker::Status::ok();
    REQUIRE(ok_status.is_ok());
    REQUIRE((bool)ok_status);

    tracker::Status err_status = tracker::Status::error(tracker::ErrorCode::CorruptHeader, "Header is corrupt");
    REQUIRE(!err_status.is_ok());
    REQUIRE_EQ(err_status.code, tracker::ErrorCode::CorruptHeader);
    REQUIRE_EQ(err_status.message, "Header is corrupt");

    tracker::Result<int> res(42);
    REQUIRE(res.is_ok());
    REQUIRE_EQ(res.value(), 42);

    tracker::Result<int> res_err(tracker::ErrorCode::UnexpectedEof, "EOF");
    REQUIRE(!res_err.is_ok());
    REQUIRE_EQ(res_err.status().code, tracker::ErrorCode::UnexpectedEof);
}
```

`tests/test_stream.cpp`:
```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -B build -S . && cmake --build build`
Expected: FAIL (missing header files and implementation)

- [ ] **Step 3: Implement minimal types, stream interface, and CMake build files**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(audio_tracker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(audio_tracker STATIC
    src/io/stream.cpp
)

target_include_directories(audio_tracker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(audio_tracker PUBLIC cxx_std_17)

enable_testing()
add_subdirectory(tests)
```

`tests/CMakeLists.txt`:
```cmake
add_executable(audio_tracker_tests
    test_main.cpp
    test_types.cpp
    test_stream.cpp
)

target_link_libraries(audio_tracker_tests PRIVATE audio_tracker)
add_test(NAME audio_tracker_tests COMMAND audio_tracker_tests)
```

`tests/test_main.cpp`:
```cpp
#include "test_main.hpp"

int main() {
    auto& tests = tracker::test::get_registry();
    std::cout << "Running " << tests.size() << " tests..." << std::endl;
    int passed = 0;
    for (const auto& test : tests) {
        std::cout << "  [RUN] " << test.name << std::endl;
        test.func();
        std::cout << "  [PASS] " << test.name << std::endl;
        passed++;
    }
    std::cout << "All " << passed << " tests passed successfully!" << std::endl;
    return 0;
}
```

`include/tracker/types.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace tracker {

enum class ErrorCode {
    Ok = 0,
    InvalidSignature,
    UnsupportedVersion,
    CorruptHeader,
    CorruptPatternData,
    CorruptInstrumentData,
    CorruptSampleData,
    UnexpectedEof,
    InvalidChannelCount,
    IoError,
    WriteError
};

struct Status {
    ErrorCode code{ErrorCode::Ok};
    std::string message;

    static Status ok() { return Status{ErrorCode::Ok, ""}; }
    static Status error(ErrorCode c, std::string msg) { return Status{c, std::move(msg)}; }

    bool is_ok() const noexcept { return code == ErrorCode::Ok; }
    explicit operator bool() const noexcept { return is_ok(); }
};

template <typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)), m_status(Status::ok()) {}
    Result(Status status) : m_status(std::move(status)) {}
    Result(ErrorCode code, std::string msg) : m_status(Status::error(code, std::move(msg))) {}

    bool is_ok() const noexcept { return m_status.is_ok(); }
    explicit operator bool() const noexcept { return is_ok(); }

    const Status& status() const noexcept { return m_status; }
    T& value() noexcept { return m_value; }
    const T& value() const noexcept { return m_value; }

private:
    T m_value{};
    Status m_status;
};

} // namespace tracker
```

`include/tracker/io/stream.hpp`:
```cpp
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
    void write_fixed_string(const std::string& str, size_t fixed_len, char pad = ' ');
    void write_zeros(size_t count);
};

} // namespace tracker::io
```

`src/io/stream.cpp`:
```cpp
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
```

- [ ] **Step 4: Build and run test suite**

Run: `cmake -B build -S . && cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 2 tests passing.

- [ ] **Step 5: Commit Task 1**

```bash
git add CMakeLists.txt include/ src/ tests/
git commit -m "feat: add core types, Status/Result, and abstract Little-Endian stream I/O"
```

---

### Task 2: Memory & File Stream Adapters

**Files:**
- Create: `include/tracker/io/memory_stream.hpp`
- Create: `include/tracker/io/file_stream.hpp`
- Create: `src/io/memory_stream.cpp`
- Create: `src/io/file_stream.cpp`
- Create: `tests/test_memory_stream.cpp`
- Create: `tests/test_file_stream.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::io::InputStream`, `tracker::io::OutputStream`.
- Produces: `tracker::io::MemoryInputStream`, `tracker::io::MemoryOutputStream`, `tracker::io::FileInputStream`, `tracker::io::FileOutputStream`.

- [ ] **Step 1: Write failing tests for memory and file stream adapters**

`tests/test_memory_stream.cpp`:
```cpp
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
```

`tests/test_file_stream.cpp`:
```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `memory_stream` and `file_stream` headers and sources)

- [ ] **Step 3: Implement memory and file stream adapters**

`include/tracker/io/memory_stream.hpp`:
```cpp
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
```

`src/io/memory_stream.cpp`:
```cpp
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
```

`include/tracker/io/file_stream.hpp`:
```cpp
#pragma once
#include <tracker/io/stream.hpp>
#include <tracker/types.hpp>
#include <cstdio>
#include <string>

namespace tracker::io {

class FileInputStream : public InputStream {
public:
    ~FileInputStream() override;
    static Result<FileInputStream> open(const std::string& path);

    FileInputStream(FileInputStream&& other) noexcept;
    FileInputStream& operator=(FileInputStream&& other) noexcept;

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
    ~FileOutputStream() override;
    static Result<FileOutputStream> open(const std::string& path);

    FileOutputStream(FileOutputStream&& other) noexcept;
    FileOutputStream& operator=(FileOutputStream&& other) noexcept;

    size_t write(const void* src, size_t num_bytes) override;
    bool seek(int64_t offset, SeekOrigin origin) override;
    int64_t tell() const override;

private:
    explicit FileOutputStream(std::FILE* fp);
    std::FILE* m_fp{nullptr};
};

} // namespace tracker::io
```

`src/io/file_stream.cpp`:
```cpp
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
```

Update `CMakeLists.txt`:
```cmake
add_library(audio_tracker STATIC
    src/io/stream.cpp
    src/io/memory_stream.cpp
    src/io/file_stream.cpp
)
```

Update `tests/CMakeLists.txt`:
```cmake
add_executable(audio_tracker_tests
    test_main.cpp
    test_types.cpp
    test_stream.cpp
    test_memory_stream.cpp
    test_file_stream.cpp
)
```

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 4 tests passing.

- [ ] **Step 5: Commit Task 2**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: add memory and file stream adapters"
```

---

### Task 3: Tracker Data Model

**Files:**
- Create: `include/tracker/model.hpp`
- Create: `src/model.cpp`
- Create: `tests/test_model.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::Cell`, `tracker::Pattern`, `tracker::Sample`, `tracker::Envelope`, `tracker::Instrument`, `tracker::Song`.

- [ ] **Step 1: Write failing tests for data model**

`tests/test_model.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/model.hpp>

TEST_CASE(Model_CellAndPattern) {
    tracker::Cell cell;
    REQUIRE(cell.is_empty());

    cell.note = 12; // C-1
    REQUIRE(!cell.is_empty());

    tracker::Pattern pat(64, 4);
    REQUIRE(pat.is_all_empty());

    pat.get_cell(10, 2).note = 24;
    REQUIRE(!pat.is_all_empty());
    REQUIRE_EQ(pat.get_cell(10, 2).note, 24);
}

TEST_CASE(Model_SongSetup) {
    tracker::Song song;
    song.name = "My Song";
    song.num_channels = 8;
    song.patterns.emplace_back(64, 8);
    song.instruments.emplace_back();

    auto& inst = song.instruments.back();
    inst.name = "Lead Synth";
    inst.samples.emplace_back();
    inst.samples.back().name = "Sawtooth";
    inst.samples.back().volume = 64;

    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.instruments.size(), 1);
    REQUIRE_EQ(song.instruments[0].samples.size(), 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `model.hpp` and implementation)

- [ ] **Step 3: Implement data model**

`include/tracker/model.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace tracker {

struct Cell {
    uint8_t note{0};         // 0: None, 1..96: Note, 97: Key-Off (0x61)
    uint8_t instrument{0};   // 0: None, 1..128: 1-indexed instrument ID
    uint8_t volume{0};       // 0: None, 0x10..0x50: Vol 0..64, 0x60..0xFF: Vol effects
    uint8_t effect_type{0};  // 0..35 (0x00..0x23)
    uint8_t effect_param{0}; // 0x00..0xFF

    bool is_empty() const noexcept {
        return note == 0 && instrument == 0 && volume == 0 &&
               effect_type == 0 && effect_param == 0;
    }
};

class Pattern {
public:
    uint16_t num_rows{64};
    uint16_t num_channels{4};
    std::vector<Cell> cells;

    Pattern() = default;
    Pattern(uint16_t rows, uint16_t channels);

    Cell& get_cell(uint16_t row, uint16_t channel);
    const Cell& get_cell(uint16_t row, uint16_t channel) const;

    bool is_all_empty() const noexcept;
};

enum class LoopType : uint8_t {
    None = 0,
    Forward = 1,
    PingPong = 2
};

struct Sample {
    std::string name;
    uint32_t length{0};          // Length in sample frames
    uint32_t loop_start{0};      // Loop start in sample frames
    uint32_t loop_length{0};     // Loop length in sample frames
    uint8_t volume{64};          // Default volume (0..64)
    int8_t finetune{0};          // Finetune (-128..+127)
    LoopType loop_type{LoopType::None};
    bool is_16bit{false};
    uint8_t panning{128};        // Panning (0..255, 128 = Center)
    int8_t relative_note{0};     // Relative note number (-96..+95)

    std::vector<int8_t> data8;
    std::vector<int16_t> data16;
};

struct EnvelopePoint {
    uint16_t tick{0};   // Frame/tick offset
    uint16_t value{0};  // Value (0..64)
};

struct Envelope {
    bool enabled{false};
    bool sustain_enabled{false};
    bool loop_enabled{false};
    uint8_t sustain_point{0};
    uint8_t loop_start_point{0};
    uint8_t loop_end_point{0};
    std::vector<EnvelopePoint> points;
};

struct Instrument {
    std::string name;
    uint8_t type{0};
    std::array<uint8_t, 96> sample_map{};
    Envelope volume_envelope;
    Envelope panning_envelope;

    uint8_t vibrato_type{0};
    uint8_t vibrato_sweep{0};
    uint8_t vibrato_depth{0};
    uint8_t vibrato_rate{0};
    uint16_t volume_fadeout{0};

    std::vector<Sample> samples;
};

struct Song {
    std::string name;
    std::string tracker_name{"FastTracker v2.00   "};
    uint16_t version{0x0104};
    uint16_t restart_position{0};
    uint16_t num_channels{4};
    bool linear_frequency{true};
    uint16_t default_speed{6};
    uint16_t default_bpm{125};

    std::vector<uint8_t> order_table;
    std::vector<Pattern> patterns;
    std::vector<Instrument> instruments;
};

} // namespace tracker
```

`src/model.cpp`:
```cpp
#include <tracker/model.hpp>

namespace tracker {

Pattern::Pattern(uint16_t rows, uint16_t channels)
    : num_rows(rows), num_channels(channels), cells(static_cast<size_t>(rows) * channels) {}

Cell& Pattern::get_cell(uint16_t row, uint16_t channel) {
    return cells[static_cast<size_t>(row) * num_channels + channel];
}

const Cell& Pattern::get_cell(uint16_t row, uint16_t channel) const {
    return cells[static_cast<size_t>(row) * num_channels + channel];
}

bool Pattern::is_all_empty() const noexcept {
    for (const auto& cell : cells) {
        if (!cell.is_empty()) return false;
    }
    return true;
}

} // namespace tracker
```

Update `CMakeLists.txt` with `src/model.cpp` and `tests/CMakeLists.txt` with `test_model.cpp`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 6 tests passing.

- [ ] **Step 5: Commit Task 3**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement unified tracker data model"
```

---

### Task 4: FastTracker II Binary Constants & Sample Delta Codec

**Files:**
- Create: `include/tracker/xm/xm_types.hpp`
- Create: `include/tracker/xm/xm_delta.hpp`
- Create: `src/xm/xm_delta.cpp`
- Create: `tests/test_xm_delta.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::xm::decode_delta_8`, `tracker::xm::encode_delta_8`, `tracker::xm::decode_delta_16`, `tracker::xm::encode_delta_16`.

- [ ] **Step 1: Write failing tests for delta codec**

`tests/test_xm_delta.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_delta.hpp>

TEST_CASE(XmDelta_8BitRoundTrip) {
    std::vector<int8_t> original = {0, 10, 20, 15, -30, -128, 127, 0};
    auto encoded = tracker::xm::encode_delta_8(original);
    auto decoded = tracker::xm::decode_delta_8(encoded);
    REQUIRE_EQ(original.size(), decoded.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(original[i], decoded[i]);
    }
}

TEST_CASE(XmDelta_16BitRoundTrip) {
    std::vector<int16_t> original = {0, 1000, 30000, -32768, 32767, -100, 0};
    auto encoded = tracker::xm::encode_delta_16(original);
    auto decoded = tracker::xm::decode_delta_16(encoded);
    REQUIRE_EQ(original.size(), decoded.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(original[i], decoded[i]);
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `xm_types.hpp` and `xm_delta` files)

- [ ] **Step 3: Implement XM binary constants and delta codec**

`include/tracker/xm/xm_types.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::xm {

constexpr const char* XM_SIGNATURE = "Extended Module: ";
constexpr size_t XM_SIGNATURE_LEN = 17;
constexpr uint8_t XM_EOF_BYTE = 0x1A;
constexpr uint16_t XM_VERSION_104 = 0x0104;

constexpr uint32_t XM_HEADER_SIZE_104 = 276;
constexpr uint32_t XM_PATTERN_HEADER_LEN = 9;
constexpr uint32_t XM_INST_HEADER_EMPTY_LEN = 29;
constexpr uint32_t XM_INST_HEADER_FULL_LEN = 263;
constexpr uint32_t XM_SAMPLE_HEADER_LEN = 40;

// Cell packing bitmasks
constexpr uint8_t XM_PACK_NOTE        = 0x01;
constexpr uint8_t XM_PACK_INSTRUMENT  = 0x02;
constexpr uint8_t XM_PACK_VOLUME      = 0x04;
constexpr uint8_t XM_PACK_EFFECT_TYPE = 0x08;
constexpr uint8_t XM_PACK_EFFECT_PARAM= 0x10;
constexpr uint8_t XM_PACK_FLAG        = 0x80;

// Sample flags
constexpr uint8_t XM_SAMPLE_LOOP_NONE     = 0x00;
constexpr uint8_t XM_SAMPLE_LOOP_FORWARD  = 0x01;
constexpr uint8_t XM_SAMPLE_LOOP_PINGPONG = 0x02;
constexpr uint8_t XM_SAMPLE_16BIT         = 0x10;

} // namespace tracker::xm
```

`include/tracker/xm/xm_delta.hpp`:
```cpp
#pragma once
#include <vector>
#include <cstdint>

namespace tracker::xm {

std::vector<int8_t> decode_delta_8(const std::vector<int8_t>& delta_data);
std::vector<int8_t> encode_delta_8(const std::vector<int8_t>& pcm_data);

std::vector<int16_t> decode_delta_16(const std::vector<int16_t>& delta_data);
std::vector<int16_t> encode_delta_16(const std::vector<int16_t>& pcm_data);

} // namespace tracker::xm
```

`src/xm/xm_delta.cpp`:
```cpp
#include <tracker/xm/xm_delta.hpp>

namespace tracker::xm {

std::vector<int8_t> decode_delta_8(const std::vector<int8_t>& delta_data) {
    std::vector<int8_t> pcm(delta_data.size());
    int8_t current = 0;
    for (size_t i = 0; i < delta_data.size(); ++i) {
        current = static_cast<int8_t>(current + delta_data[i]);
        pcm[i] = current;
    }
    return pcm;
}

std::vector<int8_t> encode_delta_8(const std::vector<int8_t>& pcm_data) {
    std::vector<int8_t> delta(pcm_data.size());
    int8_t last = 0;
    for (size_t i = 0; i < pcm_data.size(); ++i) {
        delta[i] = static_cast<int8_t>(pcm_data[i] - last);
        last = pcm_data[i];
    }
    return delta;
}

std::vector<int16_t> decode_delta_16(const std::vector<int16_t>& delta_data) {
    std::vector<int16_t> pcm(delta_data.size());
    int16_t current = 0;
    for (size_t i = 0; i < delta_data.size(); ++i) {
        current = static_cast<int16_t>(current + delta_data[i]);
        pcm[i] = current;
    }
    return pcm;
}

std::vector<int16_t> encode_delta_16(const std::vector<int16_t>& pcm_data) {
    std::vector<int16_t> delta(pcm_data.size());
    int16_t last = 0;
    for (size_t i = 0; i < pcm_data.size(); ++i) {
        delta[i] = static_cast<int16_t>(pcm_data[i] - last);
        last = pcm_data[i];
    }
    return delta;
}

} // namespace tracker::xm
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 8 tests passing.

- [ ] **Step 5: Commit Task 4**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: add XM format binary definitions and 8/16-bit delta sample codec"
```

---

### Task 5: XM Pattern Packing & Unpacking Codec

**Files:**
- Create: `include/tracker/xm/xm_pattern_codec.hpp`
- Create: `src/xm/xm_pattern_codec.cpp`
- Create: `tests/test_xm_pattern_codec.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::xm::pack_pattern`, `tracker::xm::unpack_pattern`.

- [ ] **Step 1: Write failing tests for pattern packing/unpacking**

`tests/test_xm_pattern_codec.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_pattern_codec.hpp>

TEST_CASE(XmPatternCodec_RoundTrip) {
    tracker::Pattern pat(4, 2);
    // Row 0, Ch 0: Note with inst & vol
    pat.get_cell(0, 0).note = 49;
    pat.get_cell(0, 0).instrument = 2;
    pat.get_cell(0, 0).volume = 0x40;

    // Row 1, Ch 1: Effect only
    pat.get_cell(1, 1).effect_type = 0x0A;
    pat.get_cell(1, 1).effect_param = 0x0F;

    auto packed = tracker::xm::pack_pattern(pat);
    REQUIRE(!packed.empty());

    tracker::Pattern unpacked(4, 2);
    auto status = tracker::xm::unpack_pattern(packed.data(), packed.size(), unpacked);
    REQUIRE(status.is_ok());

    REQUIRE_EQ(unpacked.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked.get_cell(0, 0).instrument, 2);
    REQUIRE_EQ(unpacked.get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_type, 0x0A);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_param, 0x0F);
}

TEST_CASE(XmPatternCodec_AllEmptyPattern) {
    tracker::Pattern pat(64, 4);
    auto packed = tracker::xm::pack_pattern(pat);
    REQUIRE(packed.empty()); // Standard FT2 optimization: empty pattern produces 0 bytes
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `xm_pattern_codec.hpp` and implementation)

- [ ] **Step 3: Implement XM pattern packing & unpacking**

`include/tracker/xm/xm_pattern_codec.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <vector>
#include <cstdint>

namespace tracker::xm {

std::vector<uint8_t> pack_pattern(const Pattern& pattern);
Status unpack_pattern(const uint8_t* packed_data, size_t packed_size, Pattern& out_pattern);

} // namespace tracker::xm
```

`src/xm/xm_pattern_codec.cpp`:
```cpp
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/xm/xm_types.hpp>

namespace tracker::xm {

std::vector<uint8_t> pack_pattern(const Pattern& pattern) {
    if (pattern.is_all_empty()) {
        return {};
    }

    std::vector<uint8_t> buffer;
    buffer.reserve(static_cast<size_t>(pattern.num_rows) * pattern.num_channels * 2);

    for (uint16_t row = 0; row < pattern.num_rows; ++row) {
        for (uint16_t ch = 0; ch < pattern.num_channels; ++ch) {
            const Cell& cell = pattern.get_cell(row, ch);
            if (cell.is_empty()) {
                buffer.push_back(XM_PACK_FLAG);
                continue;
            }

            uint8_t mask = XM_PACK_FLAG;
            if (cell.note != 0) mask |= XM_PACK_NOTE;
            if (cell.instrument != 0) mask |= XM_PACK_INSTRUMENT;
            if (cell.volume != 0) mask |= XM_PACK_VOLUME;
            if (cell.effect_type != 0) mask |= XM_PACK_EFFECT_TYPE;
            if (cell.effect_param != 0) mask |= XM_PACK_EFFECT_PARAM;

            buffer.push_back(mask);
            if (cell.note != 0) buffer.push_back(cell.note);
            if (cell.instrument != 0) buffer.push_back(cell.instrument);
            if (cell.volume != 0) buffer.push_back(cell.volume);
            if (cell.effect_type != 0) buffer.push_back(cell.effect_type);
            if (cell.effect_param != 0) buffer.push_back(cell.effect_param);
        }
    }
    return buffer;
}

Status unpack_pattern(const uint8_t* packed_data, size_t packed_size, Pattern& out_pattern) {
    size_t offset = 0;

    for (uint16_t row = 0; row < out_pattern.num_rows; ++row) {
        for (uint16_t ch = 0; ch < out_pattern.num_channels; ++ch) {
            Cell& cell = out_pattern.get_cell(row, ch);
            if (offset >= packed_size) {
                cell = Cell{};
                continue;
            }

            uint8_t b = packed_data[offset++];
            if (b & XM_PACK_FLAG) {
                if (b & XM_PACK_NOTE) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated note byte");
                    cell.note = packed_data[offset++];
                }
                if (b & XM_PACK_INSTRUMENT) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated instrument byte");
                    cell.instrument = packed_data[offset++];
                }
                if (b & XM_PACK_VOLUME) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated volume byte");
                    cell.volume = packed_data[offset++];
                }
                if (b & XM_PACK_EFFECT_TYPE) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated effect type byte");
                    cell.effect_type = packed_data[offset++];
                }
                if (b & XM_PACK_EFFECT_PARAM) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated effect param byte");
                    cell.effect_param = packed_data[offset++];
                }
            } else {
                cell.note = b;
                if (offset + 4 > packed_size) {
                    return Status::error(ErrorCode::CorruptPatternData, "Truncated uncompressed cell");
                }
                cell.instrument = packed_data[offset++];
                cell.volume = packed_data[offset++];
                cell.effect_type = packed_data[offset++];
                cell.effect_param = packed_data[offset++];
            }
        }
    }
    return Status::ok();
}

} // namespace tracker::xm
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 10 tests passing.

- [ ] **Step 5: Commit Task 5**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement XM pattern packing and unpacking"
```

---

### Task 6: XM Reader (`tracker::xm::XmReader`)

**Files:**
- Create: `include/tracker/xm/xm_reader.hpp`
- Create: `src/xm/xm_reader.cpp`
- Create: `tests/test_xm_reader.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::io::InputStream`, `tracker::Song`, `tracker::xm_delta`, `tracker::xm_pattern_codec`.
- Produces: `tracker::xm::XmReader`.

- [ ] **Step 1: Write failing tests for XmReader**

`tests/test_xm_reader.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_reader.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(XmReader_CorruptHeader) {
    std::vector<uint8_t> bad_data = {0, 1, 2, 3, 4};
    auto res = tracker::xm::XmReader::load_from_memory(bad_data.data(), bad_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `xm_reader.hpp` and implementation)

- [ ] **Step 3: Implement XM Reader**

`include/tracker/xm/xm_reader.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <string>

namespace tracker::xm {

class XmReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

} // namespace tracker::xm
```

`src/xm/xm_reader.cpp`:
```cpp
#include <tracker/xm/xm_reader.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstring>
#include <algorithm>

namespace tracker::xm {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.find_last_not_of(" \t\r\n\0");
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

Result<Song> XmReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < 60) {
        return Result<Song>(ErrorCode::InvalidSignature, "File too small for XM header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> XmReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> XmReader::load(io::InputStream& stream) {
    std::string sig = stream.read_fixed_string(XM_SIGNATURE_LEN);
    if (sig != XM_SIGNATURE) {
        return Result<Song>(ErrorCode::InvalidSignature, "Invalid XM signature: " + sig);
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(20));

    uint8_t eof_byte = stream.read_u8();
    if (eof_byte != XM_EOF_BYTE) {
        return Result<Song>(ErrorCode::CorruptHeader, "Invalid EOF byte in header");
    }

    song.tracker_name = trim_spaces(stream.read_fixed_string(20));
    song.version = stream.read_u16_le();

    int64_t header_size_pos = stream.tell();
    uint32_t header_size = stream.read_u32_le();
    int64_t pattern_start_offset = header_size_pos + header_size;

    uint16_t song_len = stream.read_u16_le();
    song.restart_position = stream.read_u16_le();
    song.num_channels = stream.read_u16_le();
    uint16_t num_patterns = stream.read_u16_le();
    uint16_t num_instruments = stream.read_u16_le();
    uint16_t flags = stream.read_u16_le();
    song.linear_frequency = ((flags & 1) != 0);
    song.default_speed = stream.read_u16_le();
    song.default_bpm = stream.read_u16_le();

    song.order_table.resize(256);
    for (size_t i = 0; i < 256; ++i) {
        song.order_table[i] = stream.read_u8();
    }
    if (song_len <= 256) {
        song.order_table.resize(song_len);
    }

    // Seek directly to start of patterns
    stream.seek(pattern_start_offset, io::SeekOrigin::Begin);

    // Read Patterns
    song.patterns.resize(num_patterns);
    for (uint16_t p = 0; p < num_patterns; ++p) {
        int64_t pat_header_start = stream.tell();
        uint32_t pat_header_len = stream.read_u32_le();
        uint8_t packing_type = stream.read_u8();
        (void)packing_type;
        uint16_t num_rows = stream.read_u16_le();
        uint16_t packed_size = stream.read_u16_le();

        Pattern pat(num_rows, song.num_channels);
        if (packed_size > 0) {
            std::vector<uint8_t> packed_data(packed_size);
            stream.read(packed_data.data(), packed_size);
            auto unpack_res = unpack_pattern(packed_data.data(), packed_size, pat);
            if (!unpack_res.is_ok()) return Result<Song>(unpack_res);
        }

        song.patterns[p] = std::move(pat);
        stream.seek(pat_header_start + pat_header_len + packed_size, io::SeekOrigin::Begin);
    }

    // Read Instruments
    song.instruments.resize(num_instruments);
    for (uint16_t inst_idx = 0; inst_idx < num_instruments; ++inst_idx) {
        int64_t inst_start = stream.tell();
        uint32_t inst_header_size = stream.read_u32_le();

        Instrument inst;
        inst.name = trim_spaces(stream.read_fixed_string(22));
        inst.type = stream.read_u8();
        uint16_t num_samples = stream.read_u16_le();

        struct RawSampleHeader {
            uint32_t length{0};
            uint32_t loop_start{0};
            uint32_t loop_length{0};
            uint8_t volume{64};
            int8_t finetune{0};
            uint8_t flags{0};
            uint8_t panning{128};
            int8_t relative_note{0};
            uint8_t reserved{0};
            std::string name;
        };

        std::vector<RawSampleHeader> raw_headers(num_samples);

        if (num_samples > 0) {
            uint32_t sample_header_size = stream.read_u32_le();
            for (size_t k = 0; k < 96; ++k) {
                inst.sample_map[k] = stream.read_u8();
            }

            // Volume Envelope Points (12 pairs of uint16_t)
            std::vector<EnvelopePoint> vol_points(12);
            for (size_t k = 0; k < 12; ++k) {
                vol_points[k].tick = stream.read_u16_le();
                vol_points[k].value = stream.read_u16_le();
            }

            // Panning Envelope Points (12 pairs of uint16_t)
            std::vector<EnvelopePoint> pan_points(12);
            for (size_t k = 0; k < 12; ++k) {
                pan_points[k].tick = stream.read_u16_le();
                pan_points[k].value = stream.read_u16_le();
            }

            uint8_t num_vol_pts = stream.read_u8();
            uint8_t num_pan_pts = stream.read_u8();
            inst.volume_envelope.sustain_point = stream.read_u8();
            inst.volume_envelope.loop_start_point = stream.read_u8();
            inst.volume_envelope.loop_end_point = stream.read_u8();
            inst.panning_envelope.sustain_point = stream.read_u8();
            inst.panning_envelope.loop_start_point = stream.read_u8();
            inst.panning_envelope.loop_end_point = stream.read_u8();

            uint8_t vol_type = stream.read_u8();
            inst.volume_envelope.enabled = ((vol_type & 1) != 0);
            inst.volume_envelope.sustain_enabled = ((vol_type & 2) != 0);
            inst.volume_envelope.loop_enabled = ((vol_type & 4) != 0);

            uint8_t pan_type = stream.read_u8();
            inst.panning_envelope.enabled = ((pan_type & 1) != 0);
            inst.panning_envelope.sustain_enabled = ((pan_type & 2) != 0);
            inst.panning_envelope.loop_enabled = ((pan_type & 4) != 0);

            inst.vibrato_type = stream.read_u8();
            inst.vibrato_sweep = stream.read_u8();
            inst.vibrato_depth = stream.read_u8();
            inst.vibrato_rate = stream.read_u8();
            inst.volume_fadeout = stream.read_u16_le();
            stream.read_u16_le(); // reserved

            vol_points.resize(std::min<size_t>(num_vol_pts, 12));
            pan_points.resize(std::min<size_t>(num_pan_pts, 12));
            inst.volume_envelope.points = std::move(vol_points);
            inst.panning_envelope.points = std::move(pan_points);

            // Seek to start of sample headers
            stream.seek(inst_start + inst_header_size, io::SeekOrigin::Begin);

            // Read Sample Headers
            for (uint16_t s = 0; s < num_samples; ++s) {
                int64_t sample_header_start = stream.tell();
                RawSampleHeader& rh = raw_headers[s];
                rh.length = stream.read_u32_le();
                rh.loop_start = stream.read_u32_le();
                rh.loop_length = stream.read_u32_le();
                rh.volume = stream.read_u8();
                rh.finetune = stream.read_i8();
                rh.flags = stream.read_u8();
                rh.panning = stream.read_u8();
                rh.relative_note = stream.read_i8();
                rh.reserved = stream.read_u8();
                rh.name = trim_spaces(stream.read_fixed_string(22));

                stream.seek(sample_header_start + sample_header_size, io::SeekOrigin::Begin);
            }

            // Read Sample Audio Data
            inst.samples.resize(num_samples);
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& rh = raw_headers[s];
                Sample sample;
                sample.name = rh.name;
                sample.volume = rh.volume;
                sample.finetune = rh.finetune;
                sample.panning = rh.panning;
                sample.relative_note = rh.relative_note;
                sample.is_16bit = ((rh.flags & XM_SAMPLE_16BIT) != 0);

                uint8_t loop_mode = (rh.flags & 3);
                if (loop_mode == XM_SAMPLE_LOOP_FORWARD) sample.loop_type = LoopType::Forward;
                else if (loop_mode == XM_SAMPLE_LOOP_PINGPONG) sample.loop_type = LoopType::PingPong;
                else sample.loop_type = LoopType::None;

                if (sample.is_16bit) {
                    uint32_t num_frames = rh.length / 2;
                    sample.length = num_frames;
                    sample.loop_start = rh.loop_start / 2;
                    sample.loop_length = rh.loop_length / 2;

                    if (rh.length > 0) {
                        std::vector<int16_t> delta_buf(num_frames);
                        for (size_t f = 0; f < num_frames; ++f) {
                            delta_buf[f] = stream.read_i16_le();
                        }
                        sample.data16 = decode_delta_16(delta_buf);
                    }
                } else {
                    sample.length = rh.length;
                    sample.loop_start = rh.loop_start;
                    sample.loop_length = rh.loop_length;

                    if (rh.length > 0) {
                        std::vector<int8_t> delta_buf(rh.length);
                        stream.read(delta_buf.data(), rh.length);
                        sample.data8 = decode_delta_8(delta_buf);
                    }
                }
                inst.samples[s] = std::move(sample);
            }
        } else {
            // Seek past empty instrument header
            stream.seek(inst_start + inst_header_size, io::SeekOrigin::Begin);
        }

        song.instruments[inst_idx] = std::move(inst);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::xm
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 11 tests passing.

- [ ] **Step 5: Commit Task 6**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement FastTracker II XM file reader"
```

---

### Task 7: XM Writer (`tracker::xm::XmWriter`)

**Files:**
- Create: `include/tracker/xm/xm_writer.hpp`
- Create: `src/xm/xm_writer.cpp`
- Create: `tests/test_xm_writer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::Song`, `tracker::io::OutputStream`, `tracker::xm_delta`, `tracker::xm_pattern_codec`.
- Produces: `tracker::xm::XmWriter`.

- [ ] **Step 1: Write failing tests for XmWriter**

`tests/test_xm_writer.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>

TEST_CASE(XmWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test Export";
    song.num_channels = 4;
    song.order_table = {0, 0};
    song.patterns.emplace_back(64, 4);

    auto res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    const auto& bytes = res.value();
    REQUIRE(bytes.size() > 60);

    // Verify signature
    std::string sig(reinterpret_cast<const char*>(bytes.data()), 17);
    REQUIRE_EQ(sig, "Extended Module: ");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `xm_writer.hpp` and implementation)

- [ ] **Step 3: Implement XM Writer**

`include/tracker/xm/xm_writer.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <vector>
#include <string>

namespace tracker::xm {

class XmWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::xm
```

`src/xm/xm_writer.cpp`:
```cpp
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>

namespace tracker::xm {

Result<std::vector<uint8_t>> XmWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status XmWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status XmWriter::save(const Song& song, io::OutputStream& stream) {
    // 1. Module Header
    stream.write(XM_SIGNATURE, XM_SIGNATURE_LEN);
    stream.write_fixed_string(song.name, 20, ' ');
    stream.write_u8(XM_EOF_BYTE);
    stream.write_fixed_string(song.tracker_name.empty() ? "FastTracker v2.00   " : song.tracker_name, 20, ' ');
    stream.write_u16_le(XM_VERSION_104);
    stream.write_u32_le(XM_HEADER_SIZE_104);

    uint16_t song_len = static_cast<uint16_t>(std::min<size_t>(song.order_table.size(), 256));
    stream.write_u16_le(song_len);
    stream.write_u16_le(song.restart_position);
    stream.write_u16_le(song.num_channels);
    stream.write_u16_le(static_cast<uint16_t>(song.patterns.size()));
    stream.write_u16_le(static_cast<uint16_t>(song.instruments.size()));

    uint16_t flags = song.linear_frequency ? 1 : 0;
    stream.write_u16_le(flags);
    stream.write_u16_le(song.default_speed);
    stream.write_u16_le(song.default_bpm);

    // 256-byte Order Table
    for (size_t i = 0; i < 256; ++i) {
        if (i < song.order_table.size()) {
            stream.write_u8(song.order_table[i]);
        } else {
            stream.write_u8(0);
        }
    }

    // 2. Patterns
    for (const auto& pat : song.patterns) {
        auto packed = pack_pattern(pat);
        stream.write_u32_le(XM_PATTERN_HEADER_LEN);
        stream.write_u8(0); // packing type
        stream.write_u16_le(pat.num_rows);
        stream.write_u16_le(static_cast<uint16_t>(packed.size()));
        if (!packed.empty()) {
            stream.write(packed.data(), packed.size());
        }
    }

    // 3. Instruments
    for (const auto& inst : song.instruments) {
        uint16_t num_samples = static_cast<uint16_t>(std::min<size_t>(inst.samples.size(), 16));
        if (num_samples == 0) {
            // Write 29-byte empty instrument header
            stream.write_u32_le(XM_INST_HEADER_EMPTY_LEN);
            stream.write_fixed_string(inst.name, 22, ' ');
            stream.write_u8(inst.type);
            stream.write_u16_le(0);
        } else {
            // Write 263-byte full instrument header
            stream.write_u32_le(XM_INST_HEADER_FULL_LEN);
            stream.write_fixed_string(inst.name, 22, ' ');
            stream.write_u8(inst.type);
            stream.write_u16_le(num_samples);
            stream.write_u32_le(XM_SAMPLE_HEADER_LEN);

            for (size_t k = 0; k < 96; ++k) {
                stream.write_u8(inst.sample_map[k]);
            }

            // Write 12 Volume Envelope Points (24 words)
            for (size_t k = 0; k < 12; ++k) {
                if (k < inst.volume_envelope.points.size()) {
                    stream.write_u16_le(inst.volume_envelope.points[k].tick);
                    stream.write_u16_le(inst.volume_envelope.points[k].value);
                } else {
                    stream.write_u16_le(0);
                    stream.write_u16_le(0);
                }
            }

            // Write 12 Panning Envelope Points (24 words)
            for (size_t k = 0; k < 12; ++k) {
                if (k < inst.panning_envelope.points.size()) {
                    stream.write_u16_le(inst.panning_envelope.points[k].tick);
                    stream.write_u16_le(inst.panning_envelope.points[k].value);
                } else {
                    stream.write_u16_le(0);
                    stream.write_u16_le(0);
                }
            }

            stream.write_u8(static_cast<uint8_t>(std::min<size_t>(inst.volume_envelope.points.size(), 12)));
            stream.write_u8(static_cast<uint8_t>(std::min<size_t>(inst.panning_envelope.points.size(), 12)));
            stream.write_u8(inst.volume_envelope.sustain_point);
            stream.write_u8(inst.volume_envelope.loop_start_point);
            stream.write_u8(inst.volume_envelope.loop_end_point);
            stream.write_u8(inst.panning_envelope.sustain_point);
            stream.write_u8(inst.panning_envelope.loop_start_point);
            stream.write_u8(inst.panning_envelope.loop_end_point);

            uint8_t vol_type = (inst.volume_envelope.enabled ? 1 : 0) |
                               (inst.volume_envelope.sustain_enabled ? 2 : 0) |
                               (inst.volume_envelope.loop_enabled ? 4 : 0);
            stream.write_u8(vol_type);

            uint8_t pan_type = (inst.panning_envelope.enabled ? 1 : 0) |
                               (inst.panning_envelope.sustain_enabled ? 2 : 0) |
                               (inst.panning_envelope.loop_enabled ? 4 : 0);
            stream.write_u8(pan_type);

            stream.write_u8(inst.vibrato_type);
            stream.write_u8(inst.vibrato_sweep);
            stream.write_u8(inst.vibrato_depth);
            stream.write_u8(inst.vibrato_rate);
            stream.write_u16_le(inst.volume_fadeout);
            stream.write_u16_le(0); // reserved

            // Sample Headers
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& sample = inst.samples[s];
                uint32_t byte_len = sample.is_16bit ? (sample.length * 2) : sample.length;
                uint32_t loop_start_bytes = sample.is_16bit ? (sample.loop_start * 2) : sample.loop_start;
                uint32_t loop_len_bytes = sample.is_16bit ? (sample.loop_length * 2) : sample.loop_length;

                stream.write_u32_le(byte_len);
                stream.write_u32_le(loop_start_bytes);
                stream.write_u32_le(loop_len_bytes);
                stream.write_u8(sample.volume);
                stream.write_i8(sample.finetune);

                uint8_t loop_mode = 0;
                if (sample.loop_type == LoopType::Forward) loop_mode = XM_SAMPLE_LOOP_FORWARD;
                else if (sample.loop_type == LoopType::PingPong) loop_mode = XM_SAMPLE_LOOP_PINGPONG;

                uint8_t flags_byte = loop_mode | (sample.is_16bit ? XM_SAMPLE_16BIT : 0);
                stream.write_u8(flags_byte);
                stream.write_u8(sample.panning);
                stream.write_i8(sample.relative_note);
                stream.write_u8(0); // reserved
                stream.write_fixed_string(sample.name, 22, ' ');
            }

            // Sample Audio Data (Delta Encoded)
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& sample = inst.samples[s];
                if (sample.is_16bit) {
                    if (!sample.data16.empty()) {
                        auto delta = encode_delta_16(sample.data16);
                        for (int16_t val : delta) {
                            stream.write_i16_le(val);
                        }
                    }
                } else {
                    if (!sample.data8.empty()) {
                        auto delta = encode_delta_8(sample.data8);
                        stream.write(delta.data(), delta.size());
                    }
                }
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::xm
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 12 tests passing.

- [ ] **Step 5: Commit Task 7**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement FastTracker II XM file writer"
```

---

### Task 8: End-to-End XM Round-Trip & Integration Verification

**Files:**
- Create: `tests/test_xm_roundtrip.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::Song`, `tracker::xm::XmWriter`, `tracker::xm::XmReader`.

- [ ] **Step 1: Write comprehensive end-to-end roundtrip test**

`tests/test_xm_roundtrip.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>

TEST_CASE(XmRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Chiptune Hero";
    original.tracker_name = "FastTracker v2.00";
    original.num_channels = 4;
    original.default_speed = 6;
    original.default_bpm = 135;
    original.linear_frequency = true;
    original.restart_position = 0;
    original.order_table = {0, 1, 0};

    // Pattern 0: Populated
    original.patterns.emplace_back(64, 4);
    original.patterns[0].get_cell(0, 0).note = 49; // C-4
    original.patterns[0].get_cell(0, 0).instrument = 1;
    original.patterns[0].get_cell(0, 0).volume = 0x40;
    original.patterns[0].get_cell(0, 0).effect_type = 0x0A;
    original.patterns[0].get_cell(0, 0).effect_param = 0x0F;

    original.patterns[0].get_cell(4, 1).note = 97; // Key off

    // Pattern 1: Empty pattern optimization
    original.patterns.emplace_back(64, 4);

    // Instrument 1: With 8-bit sample and envelopes
    original.instruments.emplace_back();
    auto& inst1 = original.instruments.back();
    inst1.name = "Lead Square";
    inst1.volume_fadeout = 256;
    inst1.volume_envelope.enabled = true;
    inst1.volume_envelope.sustain_enabled = true;
    inst1.volume_envelope.sustain_point = 1;
    inst1.volume_envelope.points = {{0, 64}, {10, 48}, {30, 0}};

    inst1.samples.emplace_back();
    auto& s1 = inst1.samples.back();
    s1.name = "Square Wave";
    s1.volume = 60;
    s1.panning = 128;
    s1.finetune = 5;
    s1.relative_note = 0;
    s1.is_16bit = false;
    s1.loop_type = tracker::LoopType::Forward;
    s1.data8 = {0, 32, 64, 32, 0, -32, -64, -32};
    s1.length = static_cast<uint32_t>(s1.data8.size());
    s1.loop_start = 0;
    s1.loop_length = s1.length;

    // Instrument 2: With 16-bit sample
    original.instruments.emplace_back();
    auto& inst2 = original.instruments.back();
    inst2.name = "16bit Pad";
    inst2.samples.emplace_back();
    auto& s2 = inst2.samples.back();
    s2.name = "Pad Wave";
    s2.volume = 55;
    s2.is_16bit = true;
    s2.loop_type = tracker::LoopType::PingPong;
    s2.data16 = {0, 5000, 15000, 30000, 15000, 0, -15000, -30000, -15000};
    s2.length = static_cast<uint32_t>(s2.data16.size());
    s2.loop_start = 2;
    s2.loop_length = 5;

    // Save to XM bytes
    auto save_res = tracker::xm::XmWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& xm_bytes = save_res.value();
    REQUIRE(!xm_bytes.empty());

    // Load back from XM bytes
    auto load_res = tracker::xm::XmReader::load_from_memory(xm_bytes.data(), xm_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();

    // Verify Metadata
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.num_channels, original.num_channels);
    REQUIRE_EQ(loaded.default_speed, original.default_speed);
    REQUIRE_EQ(loaded.default_bpm, original.default_bpm);
    REQUIRE_EQ(loaded.linear_frequency, original.linear_frequency);
    REQUIRE_EQ(loaded.order_table.size(), original.order_table.size());
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.instruments.size(), 2);

    // Verify Cells
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 49);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x0A);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x0F);
    REQUIRE_EQ(loaded.patterns[0].get_cell(4, 1).note, 97);
    REQUIRE(loaded.patterns[1].is_all_empty());

    // Verify 8-bit Sample & Envelope
    REQUIRE_EQ(loaded.instruments[0].name, original.instruments[0].name);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points.size(), 3);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points[1].tick, 10);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points[1].value, 48);

    const auto& ls1 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls1.name, s1.name);
    REQUIRE_EQ(ls1.is_16bit, false);
    REQUIRE_EQ(ls1.volume, 60);
    REQUIRE_EQ(ls1.data8.size(), s1.data8.size());
    for (size_t i = 0; i < s1.data8.size(); ++i) {
        REQUIRE_EQ(ls1.data8[i], s1.data8[i]);
    }

    // Verify 16-bit Sample
    REQUIRE_EQ(loaded.instruments[1].name, original.instruments[1].name);
    const auto& ls2 = loaded.instruments[1].samples[0];
    REQUIRE_EQ(ls2.name, s2.name);
    REQUIRE_EQ(ls2.is_16bit, true);
    REQUIRE_EQ(ls2.volume, 55);
    REQUIRE_EQ(ls2.loop_start, 2);
    REQUIRE_EQ(ls2.loop_length, 5);
    REQUIRE_EQ(ls2.data16.size(), s2.data16.size());
    for (size_t i = 0; i < s2.data16.size(); ++i) {
        REQUIRE_EQ(ls2.data16[i], s2.data16[i]);
    }
}
```

Update `tests/CMakeLists.txt` to include `test_xm_roundtrip.cpp`.

- [ ] **Step 2: Run test suite to verify everything passes**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 13 tests passing.

- [ ] **Step 3: Commit Task 8**

```bash
git add tests/ CMakeLists.txt
git commit -m "test: add full XM round-trip and fidelity integration test"
```
