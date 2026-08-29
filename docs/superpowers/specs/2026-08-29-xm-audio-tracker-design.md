# FastTracker II Extended Module (.XM) Reader & Writer C++ Library Design

## Overview
This document specifies the architecture and technical design of `audio_tracker`, a lightweight, portable C++17 library designed to read and write tracker audio module files. The initial implementation focuses on the **FastTracker II Extended Module (`.xm`)** format, with an extensible architecture designed to support `.mod`, `.s3m`, and `.it` in future iterations.

The library is specifically designed to run seamlessly on both desktop environments and resource-constrained modern microcontrollers (such as ARM Cortex-M7 / Teensy 4.x / NXP i.MX RT1176), requiring no external dependencies, no C++ exceptions (`-fno-exceptions`), and no RTTI (`-fno-rtti`).

---

## 1. Requirements and Constraints

### 1.1 Hardware and Platform Targets
* **Target Platforms:** Embedded microcontrollers (Teensy 4.0/4.1, NXP MIMXRT1176) and desktop OSs (macOS, Linux, Windows).
* **Architecture:** ARM Cortex-M (Little-Endian) and x86_64/AArch64.
* **Compiler Requirements:** C++17 compliant compiler (GCC 9+, Clang 10+, MSVC 2019+).
* **Flags Support:** Must compile cleanly with `-fno-exceptions`, `-fno-rtti`, `-Wall`, `-Wextra`, `-Werror`.
* **Zero Dependencies:** Standard C++ library only (containers and memory algorithms).

### 1.2 Functional Requirements
* **Data Model:** Generic, format-agnostic internal tracker song representation.
* **XM Deserialization (`XmReader`):** Full support for FT2 v1.04 specifications, pattern unpacking, 8-bit and 16-bit delta-decoded PCM samples, volume/panning envelopes, auto-vibrato, and empty pattern optimizations.
* **XM Serialization (`XmWriter`):** Ability to export any valid `tracker::Song` into a standard, compliant `.xm` binary file with cell packing and sample delta encoding.
* **I/O Flexibility:** Stream-based abstraction enabling reads and writes from RAM buffers, filesystem paths, or embedded storage APIs (`SdFat`, `LittleFS`).
* **Error Resilience:** Value-based status/result error reporting (`tracker::Status`, `tracker::Result<T>`) for graceful handling of corrupt, malformed, or truncated files.

---

## 2. System Architecture

```
+-------------------------------------------------------------------+
|                           User Application                        |
|             (Audio Playback Engine / Tracker Editor)              |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                     tracker::Song (Data Model)                    |
|      (Pattern, Cell, Instrument, Envelope, Sample, Orders)        |
+---------------------------------+---------------------------------+
                                  |
          +-----------------------+-----------------------+
          |                                               |
          v                                               v
+-----------------------------+             +-----------------------------+
|    tracker::xm::XmReader    |             |    tracker::xm::XmWriter    |
|   (Unpack cells, decode     |             |    (Pack cells, encode      |
|    sample deltas, envelopes)|             |    sample deltas, headers)  |
+--------------+--------------+             +--------------+--------------+
               |                                           |
               v                                           v
+-------------------------------------------------------------------+
|                    tracker::io::InputStream /                     |
|                    tracker::io::OutputStream                      |
+---------------------------------+---------------------------------+
                                  |
    +-----------------------------+-----------------------------+
    |                             |                             |
    v                             v                             v
+--------------------+   +--------------------+   +---------------------+
| MemoryInputStream /|   |  FileInputStream / |   |  Custom Embedded    |
| MemoryOutputStream |   |  FileOutputStream  |   | (SdFat / LittleFS)  |
+--------------------+   +--------------------+   +---------------------+
```

---

## 3. Data Model Specification

### 3.1 Note Cell (`tracker::Cell`)
```cpp
namespace tracker {

struct Cell {
    uint8_t note{0};         // 0: None, 1..96: Note (1=C-0, 96=B-7), 97: Key-Off (0x61)
    uint8_t instrument{0};   // 0: None, 1..128: 1-indexed instrument ID
    uint8_t volume{0};       // 0: None, 0x10..0x50: Vol 0..64, 0x60..0xFF: Vol effects
    uint8_t effect_type{0};  // 0..35 (0x00..0x23)
    uint8_t effect_param{0}; // 0x00..0xFF

    bool is_empty() const noexcept {
        return note == 0 && instrument == 0 && volume == 0 &&
               effect_type == 0 && effect_param == 0;
    }
};

} // namespace tracker
```

### 3.2 Pattern (`tracker::Pattern`)
```cpp
namespace tracker {

class Pattern {
public:
    uint16_t num_rows{64};
    uint16_t num_channels{4};
    std::vector<Cell> cells; // Flattened 2D array [row * num_channels + channel]

    Pattern() = default;
    Pattern(uint16_t rows, uint16_t channels)
        : num_rows(rows), num_channels(channels), cells(static_cast<size_t>(rows) * channels) {}

    Cell& get_cell(uint16_t row, uint16_t channel) {
        return cells[static_cast<size_t>(row) * num_channels + channel];
    }

    const Cell& get_cell(uint16_t row, uint16_t channel) const {
        return cells[static_cast<size_t>(row) * num_channels + channel];
    }

    bool is_all_empty() const noexcept {
        for (const auto& cell : cells) {
            if (!cell.is_empty()) return false;
        }
        return true;
    }
};

} // namespace tracker
```

### 3.3 Sample (`tracker::Sample`)
```cpp
namespace tracker {

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
    int8_t relative_note{0};     // Relative note number (-96..+95 semitones)

    // Uncompressed signed linear PCM sample buffers in host memory
    std::vector<int8_t> data8;
    std::vector<int16_t> data16;
};

} // namespace tracker
```

### 3.4 Envelopes & Instrument (`tracker::Envelope`, `tracker::Instrument`)
```cpp
namespace tracker {

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
    std::vector<EnvelopePoint> points; // Up to 12 points
};

struct Instrument {
    std::string name;
    uint8_t type{0};
    std::array<uint8_t, 96> sample_map{}; // Maps note 0..95 -> sample index (0..15)
    Envelope volume_envelope;
    Envelope panning_envelope;

    // Auto-vibrato
    uint8_t vibrato_type{0};  // 0: Sine, 1: Square, 2: Ramp Down, 3: Ramp Up
    uint8_t vibrato_sweep{0};
    uint8_t vibrato_depth{0};
    uint8_t vibrato_rate{0};
    uint16_t volume_fadeout{0};

    std::vector<Sample> samples; // Up to 16 samples per instrument
};

} // namespace tracker
```

### 3.5 Song / Module (`tracker::Song`)
```cpp
namespace tracker {

struct Song {
    std::string name;
    std::string tracker_name{"FastTracker v2.00   "};
    uint16_t version{0x0104};
    uint16_t restart_position{0};
    uint16_t num_channels{4};
    bool linear_frequency{true}; // true: Linear table, false: Amiga table
    uint16_t default_speed{6};   // Ticks per row
    uint16_t default_bpm{125};   // Beats per minute

    std::vector<uint8_t> order_table;    // Pattern sequence table
    std::vector<Pattern> patterns;       // List of patterns
    std::vector<Instrument> instruments; // List of instruments
};

} // namespace tracker
```

---

## 4. I/O Stream & Error Handling Abstraction

### 4.1 Status & Result
```cpp
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

### 4.2 Abstract Streams (`InputStream`, `OutputStream`)
All binary multi-byte integers are read and written strictly in Little-Endian byte order.

```cpp
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

    // Typed Little-Endian read helpers
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

    // Typed Little-Endian write helpers
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

---

## 5. FastTracker II (.XM) Serialization & Deserialization

### 5.1 Binary Header Layout Constants
* Header ID: `"Extended Module: "` (17 bytes)
* EOF Byte: `0x1A`
* Version: `0x0104` (FT2 v1.04)
* Standard Module Header Size: `276` (`0x0114`) bytes from offset 60
* Standard Pattern Header Size: `9` bytes
* Standard Empty Instrument Header Size: `29` bytes
* Standard Instrument Header Size with Samples: `263` bytes
* Standard Sample Header Size: `40` bytes

### 5.2 Pattern Cell Packing Format
* High bit `0x80`: Packed byte indicator.
  * Bit 0 (`0x01`): Note follows (`1..96` note, `97` key-off)
  * Bit 1 (`0x02`): Instrument follows (`1..128`)
  * Bit 2 (`0x04`): Volume column follows (`0x10..0xFF`)
  * Bit 3 (`0x08`): Effect type follows (`0..35`)
  * Bit 4 (`0x10`): Effect parameter follows (`0x00..0xFF`)
* If high bit is 0: Note byte itself, followed uncompressed by instrument, volume, effect type, effect parameter.
* **Empty Pattern Optimization:** When saving, if all cells in a pattern are empty, `packed_patterndata_size = 0` is written, omitting packed bytes completely.

### 5.3 Sample Delta Encoding
XM samples are stored differentials:
* **Decoding:**
  $$\text{pcm}[i] = \text{pcm}[i-1] + \text{delta}[i] \quad (\text{with } \text{pcm}[-1] = 0)$$
  Using standard 8-bit or 16-bit signed wrap-around addition.
* **Encoding:**
  $$\text{delta}[i] = \text{pcm}[i] - \text{pcm}[i-1] \quad (\text{with } \text{pcm}[-1] = 0)$$

---

## 6. Verification and Testing Strategy

### 6.1 Automated Test Suite
1. **`test_stream`**: Verifies `MemoryInputStream`, `MemoryOutputStream`, `FileInputStream`, `FileOutputStream`, seeking bounds, typed endian helpers, string padding.
2. **`test_model`**: Verifies `Cell`, `Pattern`, `Sample`, `Envelope`, `Instrument`, and `Song` defaults and cell indexing.
3. **`test_delta_codec`**: Verifies 8-bit and 16-bit differential PCM encoding/decoding, overflow wrapping, and large audio buffers.
4. **`test_xm_roundtrip`**:
   * Creates synthetic songs with multi-channel patterns, volume effects, arpeggios, envelopes, and 8-bit/16-bit samples.
   * Serializes to binary `.xm` in memory.
   * Deserializes back to `tracker::Song`.
   * Asserts exact identity of all notes, instruments, envelope points, and audio sample buffers.
5. **`test_xm_robustness`**: Validates graceful failure on corrupt headers, invalid signatures, truncated files, and zero-sample instruments.

---
