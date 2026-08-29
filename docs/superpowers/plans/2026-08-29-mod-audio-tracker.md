# ProTracker / FastTracker (.MOD) Reader & Writer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the `audio_tracker` C++17 library to read and write Commodore Amiga / ProTracker / FastTracker Module (`.mod`) files (31 samples, 4 to 32 channels) with zero external dependencies, seamless compatibility with modern microcontrollers (Teensy 4.x / NXP i.MX RT1176), and full bidirectional `.xm` <-> `.mod` cross-format conversion.

**Architecture:** Add Big-Endian stream I/O methods to `tracker::io::InputStream`/`OutputStream`, implement Amiga PAL period conversion tables and 4-byte packed note cell codecs in `tracker::mod`, and implement `ModReader` and `ModWriter` interfacing directly with the unified `tracker::Song` data model.

**Tech Stack:** C++17, CMake 3.16+, `-fno-exceptions`, `-fno-rtti`, Motorola 68000 Big-Endian binary I/O, custom zero-dependency unit test runner.

**Spec:** [`docs/superpowers/specs/2026-08-29-mod-audio-tracker-design.md`](file:///Users/moolet/Development/github/newdigate/audio-tracker/docs/superpowers/specs/2026-08-29-mod-audio-tracker-design.md)

## Global Constraints
* Language: C++17 (`cxx_std_17`).
* No external dependencies beyond the standard library (C++ STL containers).
* Zero C++ exceptions (`-fno-exceptions`) and zero RTTI (`-fno-rtti`) compatible.
* Motorola 68000 Big-Endian binary multi-byte integer serialization for `.mod` files.
* Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

---

### Task 1: Big-Endian Stream I/O Helpers

**Files:**
- Modify: `include/tracker/io/stream.hpp`
- Modify: `src/io/stream.cpp`
- Create: `tests/test_stream_be.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `InputStream::read_u16_be()`, `InputStream::read_i16_be()`, `InputStream::read_u32_be()`, `OutputStream::write_u16_be()`, `OutputStream::write_i16_be()`, `OutputStream::write_u32_be()`.

- [ ] **Step 1: Write failing tests for Big-Endian stream helpers**

`tests/test_stream_be.cpp`:
```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (methods not declared in `stream.hpp`)

- [ ] **Step 3: Implement Big-Endian stream helpers**

Update `include/tracker/io/stream.hpp`:
```cpp
// Add to InputStream:
uint16_t read_u16_be();
int16_t  read_i16_be();
uint32_t read_u32_be();

// Add to OutputStream:
void write_u16_be(uint16_t val);
void write_i16_be(int16_t val);
void write_u32_be(uint32_t val);
```

Update `src/io/stream.cpp`:
```cpp
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
```

Update `tests/CMakeLists.txt` to include `test_stream_be.cpp`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 55 tests passing.

- [ ] **Step 5: Commit Task 1**

```bash
git add include/ src/ tests/
git commit -m "feat: add Big-Endian multi-byte stream I/O helpers"
```

---

### Task 2: Amiga Period Tables & 4-Byte Cell Codec

**Files:**
- Create: `include/tracker/mod/mod_types.hpp`
- Create: `include/tracker/mod/mod_cell.hpp`
- Create: `src/mod/mod_cell.cpp`
- Create: `tests/test_mod_cell.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::mod::AMIGA_PERIOD_TABLE`, `tracker::mod::period_to_note`, `tracker::mod::note_to_period`, `tracker::mod::unpack_cell`, `tracker::mod::pack_cell`.

- [ ] **Step 1: Write failing tests for MOD period table & cell codec**

`tests/test_mod_cell.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/mod/mod_cell.hpp>
#include <tracker/mod/mod_types.hpp>

TEST_CASE(ModCell_PeriodToNote) {
    REQUIRE_EQ(tracker::mod::period_to_note(0), 0);
    REQUIRE_EQ(tracker::mod::period_to_note(856), 13); // C-1
    REQUIRE_EQ(tracker::mod::period_to_note(428), 25); // C-2
    REQUIRE_EQ(tracker::mod::period_to_note(214), 37); // C-3
    REQUIRE_EQ(tracker::mod::period_to_note(113), 48); // B-3

    // Closest match snapping
    REQUIRE_EQ(tracker::mod::period_to_note(850), 13); // close to 856
    REQUIRE_EQ(tracker::mod::period_to_note(810), 14); // close to 808
}

TEST_CASE(ModCell_NoteToPeriod) {
    REQUIRE_EQ(tracker::mod::note_to_period(0), 0);
    REQUIRE_EQ(tracker::mod::note_to_period(13), 856);
    REQUIRE_EQ(tracker::mod::note_to_period(25), 428);
    REQUIRE_EQ(tracker::mod::note_to_period(97), 0); // Key off has no period
}

TEST_CASE(ModCell_PackAndUnpack) {
    tracker::Cell cell;
    cell.note = 13; // C-1 (period 856 = 0x0358)
    cell.instrument = 18; // 0x12
    cell.effect_type = 0x0C; // Set volume
    cell.effect_param = 0x3F;

    uint8_t raw[4] = {0};
    tracker::mod::pack_cell(cell, raw);

    // Byte 0: Sample high nibble (0x10) | Period high nibble (0x03) = 0x13
    REQUIRE_EQ(raw[0], 0x13);
    // Byte 1: Period low 8 bits (0x58) = 0x58
    REQUIRE_EQ(raw[1], 0x58);
    // Byte 2: Sample low nibble (0x20) | Effect (0x0C) = 0x2C
    REQUIRE_EQ(raw[2], 0x2C);
    // Byte 3: Param = 0x3F
    REQUIRE_EQ(raw[3], 0x3F);

    tracker::Cell unpacked;
    tracker::mod::unpack_cell(raw, unpacked);
    REQUIRE_EQ(unpacked.note, 13);
    REQUIRE_EQ(unpacked.instrument, 18);
    REQUIRE_EQ(unpacked.effect_type, 0x0C);
    REQUIRE_EQ(unpacked.effect_param, 0x3F);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `mod_types.hpp` and `mod_cell.hpp`)

- [ ] **Step 3: Implement MOD types and cell codec**

`include/tracker/mod/mod_types.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::mod {

constexpr size_t MOD_TITLE_LEN = 20;
constexpr size_t MOD_SAMPLE_NAME_LEN = 22;
constexpr size_t MOD_NUM_SAMPLES = 31;
constexpr size_t MOD_SAMPLE_HEADER_LEN = 30;
constexpr size_t MOD_ORDER_TABLE_LEN = 128;
constexpr size_t MOD_HEADER_LEN = 1084;
constexpr size_t MOD_PATTERN_ROWS = 64;

// Standard Amiga PAL period table for 5 octaves (C-0 to B-5, 60 notes)
constexpr uint16_t AMIGA_PERIOD_TABLE[60] = {
    // Octave 0: C-0 to B-0 (Notes 1..12)
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906,
    // Octave 1: C-1 to B-1 (Notes 13..24)
    856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480, 453,
    // Octave 2: C-2 to B-2 (Notes 25..36)
    428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240, 226,
    // Octave 3: C-3 to B-3 (Notes 37..48)
    214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120, 113,
    // Octave 4: C-4 to B-4 (Notes 49..60)
    107,  101,  95,   90,   85,   80,   75,   71,   67,   63,   60,  56
};

} // namespace tracker::mod
```

`include/tracker/mod/mod_cell.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <cstdint>

namespace tracker::mod {

uint8_t period_to_note(uint16_t period) noexcept;
uint16_t note_to_period(uint8_t note) noexcept;

void unpack_cell(const uint8_t raw[4], Cell& out_cell) noexcept;
void pack_cell(const Cell& cell, uint8_t out_raw[4]) noexcept;

} // namespace tracker::mod
```

`src/mod/mod_cell.cpp`:
```cpp
#include <tracker/mod/mod_cell.hpp>
#include <tracker/mod/mod_types.hpp>
#include <cmath>
#include <cstdlib>

namespace tracker::mod {

uint8_t period_to_note(uint16_t period) noexcept {
    if (period == 0) return 0;

    uint8_t best_note = 1;
    int best_diff = std::abs(static_cast<int>(period) - static_cast<int>(AMIGA_PERIOD_TABLE[0]));

    for (uint8_t i = 1; i < 60; ++i) {
        int diff = std::abs(static_cast<int>(period) - static_cast<int>(AMIGA_PERIOD_TABLE[i]));
        if (diff < best_diff) {
            best_diff = diff;
            best_note = i + 1;
        }
    }
    return best_note;
}

uint16_t note_to_period(uint8_t note) noexcept {
    if (note >= 1 && note <= 60) {
        return AMIGA_PERIOD_TABLE[note - 1];
    }
    return 0;
}

void unpack_cell(const uint8_t raw[4], Cell& out_cell) noexcept {
    uint8_t sample_num = (raw[0] & 0xF0) | (raw[2] >> 4);
    uint16_t period = (static_cast<uint16_t>(raw[0] & 0x0F) << 8) | raw[1];
    uint8_t effect = raw[2] & 0x0F;
    uint8_t param = raw[3];

    out_cell.instrument = sample_num;
    out_cell.note = period_to_note(period);
    out_cell.volume = 0;
    out_cell.effect_type = effect;
    out_cell.effect_param = param;
}

void pack_cell(const Cell& cell, uint8_t out_raw[4]) noexcept {
    uint16_t period = note_to_period(cell.note);
    uint8_t sample = cell.instrument;
    uint8_t effect = cell.effect_type & 0x0F;
    uint8_t param = cell.effect_param;

    out_raw[0] = (sample & 0xF0) | static_cast<uint8_t>((period >> 8) & 0x0F);
    out_raw[1] = static_cast<uint8_t>(period & 0xFF);
    out_raw[2] = static_cast<uint8_t>((sample & 0x0F) << 4) | (effect & 0x0F);
    out_raw[3] = param;
}

} // namespace tracker::mod
```

Update `CMakeLists.txt` with `src/mod/mod_cell.cpp` and `tests/CMakeLists.txt` with `test_mod_cell.cpp`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 58 tests passing.

- [ ] **Step 5: Commit Task 2**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: add MOD Amiga period tables and 4-byte packed cell codec"
```

---

### Task 3: MOD Reader (`tracker::mod::ModReader`)

**Files:**
- Create: `include/tracker/mod/mod_reader.hpp`
- Create: `src/mod/mod_reader.cpp`
- Create: `tests/test_mod_reader.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::io::InputStream`, `tracker::Song`, `tracker::mod::unpack_cell`.
- Produces: `tracker::mod::ModReader`.

- [ ] **Step 1: Write failing tests for ModReader**

`tests/test_mod_reader.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/mod/mod_reader.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(ModReader_InvalidHeader) {
    std::vector<uint8_t> short_data(100, 0);
    auto res = tracker::mod::ModReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(ModReader_Minimal4ChannelModule) {
    std::vector<uint8_t> mod_data(1084 + (64 * 4 * 4), 0);
    // Set song title
    std::memcpy(mod_data.data(), "Test MOD", 8);
    // Set song length
    mod_data[950] = 1;
    // Set pattern 0 in order table
    mod_data[952] = 0;
    // Set "M.K." signature at offset 1080
    mod_data[1080] = 'M';
    mod_data[1081] = '.';
    mod_data[1082] = 'K';
    mod_data[1083] = '.';

    // Cell at row 0, ch 0: C-1 on Sample 1
    // C-1 period = 856 = 0x0358, sample 1 = 0x01
    mod_data[1084] = 0x03;
    mod_data[1085] = 0x58;
    mod_data[1086] = 0x10;
    mod_data[1087] = 0x00;

    auto res = tracker::mod::ModReader::load_from_memory(mod_data.data(), mod_data.size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.name, "Test MOD");
    REQUIRE_EQ(song.num_channels, 4);
    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).note, 13);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).instrument, 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `mod_reader.hpp` and implementation)

- [ ] **Step 3: Implement MOD Reader**

`include/tracker/mod/mod_reader.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <string>

namespace tracker::mod {

class ModReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

} // namespace tracker::mod
```

`src/mod/mod_reader.cpp`:
```cpp
#include <tracker/mod/mod_reader.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/mod/mod_cell.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstring>
#include <algorithm>

namespace tracker::mod {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.find_last_not_of(" \t\r\n\0");
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

static uint16_t parse_channel_count(const std::string& tag) {
    if (tag == "M.K." || tag == "M!K!" || tag == "4CHN" || tag == "FLT4") return 4;
    if (tag == "6CHN") return 6;
    if (tag == "8CHN" || tag == "OCTA" || tag == "CD81" || tag == "FLT8") return 8;
    if (tag == "16CN") return 16;
    if (tag == "32CN") return 32;

    if (tag.size() == 4) {
        if (tag[2] == 'C' && (tag[3] == 'H' || tag[3] == 'N')) {
            int ch = (tag[0] - '0') * 10 + (tag[1] - '0');
            if (ch > 0 && ch <= 32) return static_cast<uint16_t>(ch);
        } else if (tag[0] >= '1' && tag[0] <= '9' && tag.substr(1) == "CHN") {
            return static_cast<uint16_t>(tag[0] - '0');
        }
    }
    return 0;
}

Result<Song> ModReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < MOD_HEADER_LEN) {
        return Result<Song>(ErrorCode::InvalidSignature, "Data too small for MOD header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> ModReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> ModReader::load(io::InputStream& stream) {
    if (stream.size() > 0 && stream.size() < static_cast<int64_t>(MOD_HEADER_LEN)) {
        return Result<Song>(ErrorCode::InvalidSignature, "Stream too small for MOD header");
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(MOD_TITLE_LEN));

    struct RawSampleInfo {
        std::string name;
        uint32_t length_bytes{0};
        int8_t finetune{0};
        uint8_t volume{64};
        uint32_t loop_start_bytes{0};
        uint32_t loop_length_bytes{0};
    };

    std::vector<RawSampleInfo> raw_samples(MOD_NUM_SAMPLES);
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        RawSampleInfo& info = raw_samples[s];
        info.name = trim_spaces(stream.read_fixed_string(MOD_SAMPLE_NAME_LEN));
        uint16_t len_words = stream.read_u16_be();
        info.length_bytes = static_cast<uint32_t>(len_words) * 2;

        uint8_t ft = stream.read_u8() & 0x0F;
        info.finetune = (ft >= 8) ? static_cast<int8_t>(ft - 16) : static_cast<int8_t>(ft);
        info.volume = std::min<uint8_t>(stream.read_u8(), 64);

        uint16_t lstart_words = stream.read_u16_be();
        info.loop_start_bytes = static_cast<uint32_t>(lstart_words) * 2;
        uint16_t llen_words = stream.read_u16_be();
        info.loop_length_bytes = static_cast<uint32_t>(llen_words) * 2;
    }

    uint8_t song_len = stream.read_u8();
    uint8_t restart_pos = stream.read_u8();
    song.restart_position = restart_pos;

    std::vector<uint8_t> raw_orders(MOD_ORDER_TABLE_LEN);
    stream.read(raw_orders.data(), MOD_ORDER_TABLE_LEN);

    uint8_t effective_len = std::min<uint8_t>(song_len, 128);
    if (effective_len == 0) effective_len = 1;
    song.order_table.assign(raw_orders.begin(), raw_orders.begin() + effective_len);

    std::string tag = stream.read_fixed_string(4);
    uint16_t num_channels = parse_channel_count(tag);
    if (num_channels == 0) {
        return Result<Song>(ErrorCode::InvalidSignature, "Unknown MOD signature: " + tag);
    }
    song.num_channels = num_channels;
    song.linear_frequency = false;
    song.default_speed = 6;
    song.default_bpm = 125;

    // Find highest pattern index in order table
    uint8_t max_pat_idx = 0;
    for (uint8_t p : song.order_table) {
        if (p > max_pat_idx) max_pat_idx = p;
    }
    uint16_t num_patterns = static_cast<uint16_t>(max_pat_idx + 1);

    // Read Patterns
    song.patterns.resize(num_patterns);
    for (uint16_t p = 0; p < num_patterns; ++p) {
        Pattern pat(MOD_PATTERN_ROWS, song.num_channels);
        for (uint16_t row = 0; row < MOD_PATTERN_ROWS; ++row) {
            for (uint16_t ch = 0; ch < song.num_channels; ++ch) {
                uint8_t cell_bytes[4] = {0, 0, 0, 0};
                if (stream.read(cell_bytes, 4) < 4) {
                    return Result<Song>(ErrorCode::CorruptPatternData, "Truncated pattern data");
                }
                unpack_cell(cell_bytes, pat.get_cell(row, ch));
            }
        }
        song.patterns[p] = std::move(pat);
    }

    // Read Sample Data
    song.instruments.resize(MOD_NUM_SAMPLES);
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        const auto& raw_info = raw_samples[s];
        Instrument inst;
        inst.name = raw_info.name;

        if (raw_info.length_bytes > 0) {
            Sample sample;
            sample.name = raw_info.name;
            sample.length = raw_info.length_bytes;
            sample.volume = raw_info.volume;
            sample.finetune = raw_info.finetune;
            sample.is_16bit = false;

            if (raw_info.loop_length_bytes > 2) {
                sample.loop_type = LoopType::Forward;
                sample.loop_start = raw_info.loop_start_bytes;
                sample.loop_length = raw_info.loop_length_bytes;
            } else {
                sample.loop_type = LoopType::None;
                sample.loop_start = 0;
                sample.loop_length = 0;
            }

            sample.data8.resize(raw_info.length_bytes);
            if (stream.read(sample.data8.data(), raw_info.length_bytes) < raw_info.length_bytes) {
                // If stream terminates early, preserve whatever was read
            }
            inst.samples.push_back(std::move(sample));
        }
        song.instruments[s] = std::move(inst);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::mod
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 60 tests passing.

- [ ] **Step 5: Commit Task 3**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement ProTracker MOD file reader"
```

---

### Task 4: MOD Writer (`tracker::mod::ModWriter`)

**Files:**
- Create: `include/tracker/mod/mod_writer.hpp`
- Create: `src/mod/mod_writer.cpp`
- Create: `tests/test_mod_writer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::Song`, `tracker::io::OutputStream`, `tracker::mod::pack_cell`.
- Produces: `tracker::mod::ModWriter`.

- [ ] **Step 1: Write failing tests for ModWriter**

`tests/test_mod_writer.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/mod/mod_writer.hpp>

TEST_CASE(ModWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Export Mod";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    auto res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    const auto& bytes = res.value();
    REQUIRE(bytes.size() >= 1084 + (64 * 4 * 4));

    // Verify signature at offset 1080
    std::string tag(reinterpret_cast<const char*>(bytes.data() + 1080), 4);
    REQUIRE_EQ(tag, "M.K.");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `mod_writer.hpp` and implementation)

- [ ] **Step 3: Implement MOD Writer**

`include/tracker/mod/mod_writer.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <vector>
#include <string>

namespace tracker::mod {

class ModWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::mod
```

`src/mod/mod_writer.cpp`:
```cpp
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/mod/mod_cell.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>

namespace tracker::mod {

Result<std::vector<uint8_t>> ModWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status ModWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status ModWriter::save(const Song& song, io::OutputStream& stream) {
    // 1. Song Title (20 bytes)
    stream.write_fixed_string(song.name, MOD_TITLE_LEN, '\0');

    // 2. 31 Sample Headers (30 bytes each)
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        if (s < song.instruments.size() && !song.instruments[s].samples.empty()) {
            const auto& sample = song.instruments[s].samples[0];
            stream.write_fixed_string(sample.name, MOD_SAMPLE_NAME_LEN, '\0');

            uint32_t len_bytes = sample.is_16bit ? (sample.length * 2) : sample.length;
            uint16_t len_words = static_cast<uint16_t>(len_bytes / 2);
            stream.write_u16_be(len_words);

            uint8_t ft_nibble = static_cast<uint8_t>(sample.finetune & 0x0F);
            stream.write_u8(ft_nibble);
            stream.write_u8(std::min<uint8_t>(sample.volume, 64));

            if (sample.loop_type != LoopType::None && sample.loop_length > 0) {
                uint32_t lstart_bytes = sample.is_16bit ? (sample.loop_start * 2) : sample.loop_start;
                uint32_t llen_bytes = sample.is_16bit ? (sample.loop_length * 2) : sample.loop_length;
                stream.write_u16_be(static_cast<uint16_t>(lstart_bytes / 2));
                stream.write_u16_be(static_cast<uint16_t>(llen_bytes / 2));
            } else {
                stream.write_u16_be(0);
                stream.write_u16_be(1); // 1 word = standard ProTracker loop-off marker
            }
        } else {
            stream.write_zeros(MOD_SAMPLE_NAME_LEN);
            stream.write_u16_be(0); // length 0
            stream.write_u8(0);     // finetune 0
            stream.write_u8(0);     // volume 0
            stream.write_u16_be(0); // loop start 0
            stream.write_u16_be(1); // loop length 1
        }
    }

    // 3. Song Length & Restart Position
    uint8_t song_len = static_cast<uint8_t>(std::min<size_t>(song.order_table.size(), MOD_ORDER_TABLE_LEN));
    if (song_len == 0) song_len = 1;
    stream.write_u8(song_len);
    stream.write_u8(static_cast<uint8_t>(song.restart_position));

    // 4. 128-byte Order Table
    for (size_t i = 0; i < MOD_ORDER_TABLE_LEN; ++i) {
        if (i < song.order_table.size()) {
            stream.write_u8(song.order_table[i]);
        } else {
            stream.write_u8(0);
        }
    }

    // 5. Signature Tag (4 bytes)
    if (song.num_channels == 4) {
        stream.write("M.K.", 4);
    } else if (song.num_channels == 6) {
        stream.write("6CHN", 4);
    } else if (song.num_channels == 8) {
        stream.write("8CHN", 4);
    } else if (song.num_channels < 10) {
        std::string tag = std::to_string(song.num_channels) + "CHN";
        stream.write(tag.data(), 4);
    } else {
        std::string tag = std::to_string(song.num_channels) + "CN";
        stream.write(tag.data(), 4);
    }

    // 6. Pattern Data
    for (const auto& pat : song.patterns) {
        for (uint16_t row = 0; row < MOD_PATTERN_ROWS; ++row) {
            for (uint16_t ch = 0; ch < song.num_channels; ++ch) {
                uint8_t raw[4] = {0, 0, 0, 0};
                if (row < pat.num_rows && ch < pat.num_channels) {
                    pack_cell(pat.get_cell(row, ch), raw);
                }
                stream.write(raw, 4);
            }
        }
    }

    // 7. Sample Audio Data (Raw Signed 8-bit PCM)
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        if (s < song.instruments.size() && !song.instruments[s].samples.empty()) {
            const auto& sample = song.instruments[s].samples[0];
            if (sample.is_16bit) {
                for (int16_t val : sample.data16) {
                    int8_t val8 = static_cast<int8_t>(val >> 8);
                    stream.write_i8(val8);
                }
            } else {
                if (!sample.data8.empty()) {
                    stream.write(sample.data8.data(), sample.data8.size());
                }
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::mod
```

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 62 tests passing.

- [ ] **Step 5: Commit Task 4**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement ProTracker MOD file writer"
```

---

### Task 5: End-to-End MOD Round-Trip & Bidirectional Cross-Format Conversion

**Files:**
- Create: `tests/test_mod_roundtrip.cpp`
- Create: `tests/test_cross_format.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::mod::ModWriter`, `tracker::mod::ModReader`, `tracker::xm::XmWriter`, `tracker::xm::XmReader`.

- [ ] **Step 1: Write comprehensive MOD round-trip and cross-format tests**

`tests/test_mod_roundtrip.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>

TEST_CASE(ModRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Amiga Classic";
    original.num_channels = 4;
    original.order_table = {0, 1, 0};

    // Pattern 0
    original.patterns.emplace_back(64, 4);
    original.patterns[0].get_cell(0, 0).note = 13; // C-1
    original.patterns[0].get_cell(0, 0).instrument = 1;
    original.patterns[0].get_cell(0, 0).effect_type = 0x0C; // Set volume
    original.patterns[0].get_cell(0, 0).effect_param = 0x30;

    // Pattern 1
    original.patterns.emplace_back(64, 4);
    original.patterns[1].get_cell(10, 2).note = 25; // C-2
    original.patterns[1].get_cell(10, 2).instrument = 2;

    // Instrument 1: Sample with loop
    original.instruments.emplace_back();
    auto& inst1 = original.instruments.back();
    inst1.name = "Bass";
    inst1.samples.emplace_back();
    auto& s1 = inst1.samples.back();
    s1.name = "Bass Sample";
    s1.volume = 58;
    s1.finetune = 2;
    s1.loop_type = tracker::LoopType::Forward;
    s1.data8 = {0, 10, 20, 10, 0, -10, -20, -10};
    s1.length = static_cast<uint32_t>(s1.data8.size());
    s1.loop_start = 2;
    s1.loop_length = 4;

    // Instrument 2: Unlooped sample
    original.instruments.emplace_back();
    auto& inst2 = original.instruments.back();
    inst2.name = "Snare";
    inst2.samples.emplace_back();
    auto& s2 = inst2.samples.back();
    s2.name = "Snare Hit";
    s2.volume = 64;
    s2.data8 = {30, -30, 20, -20, 10, -10};
    s2.length = static_cast<uint32_t>(s2.data8.size());

    // Save to MOD memory buffer
    auto save_res = tracker::mod::ModWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& mod_bytes = save_res.value();
    REQUIRE(!mod_bytes.empty());

    // Load back from MOD memory buffer
    auto load_res = tracker::mod::ModReader::load_from_memory(mod_bytes.data(), mod_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.num_channels, 4);
    REQUIRE_EQ(loaded.order_table.size(), 3);
    REQUIRE_EQ(loaded.patterns.size(), 2);

    // Verify cell contents
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 13);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x0C);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x30);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 2).note, 25);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 2).instrument, 2);

    // Verify sample 1
    const auto& ls1 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls1.name, s1.name);
    REQUIRE_EQ(ls1.volume, 58);
    REQUIRE_EQ(ls1.finetune, 2);
    REQUIRE_EQ(ls1.loop_type, tracker::LoopType::Forward);
    REQUIRE_EQ(ls1.loop_start, 2);
    REQUIRE_EQ(ls1.loop_length, 4);
    REQUIRE_EQ(ls1.data8.size(), s1.data8.size());
    for (size_t i = 0; i < s1.data8.size(); ++i) {
        REQUIRE_EQ(ls1.data8[i], s1.data8[i]);
    }
}
```

`tests/test_cross_format.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>

TEST_CASE(CrossFormat_XmToModToXm) {
    // 1. Create Song
    tracker::Song orig;
    orig.name = "Cross Platform";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25; // C-2
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "Sine";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "Sine 8bit";
    s.volume = 64;
    s.data8 = {0, 32, 64, 32, 0, -32, -64, -32};
    s.length = static_cast<uint32_t>(s.data8.size());

    // 2. Export to XM bytes
    auto xm_bytes_res = tracker::xm::XmWriter::save_to_memory(orig);
    REQUIRE(xm_bytes_res.is_ok());

    // 3. Load XM -> Export to MOD
    auto xm_song = tracker::xm::XmReader::load_from_memory(xm_bytes_res.value().data(), xm_bytes_res.value().size()).value();
    auto mod_bytes_res = tracker::mod::ModWriter::save_to_memory(xm_song);
    REQUIRE(mod_bytes_res.is_ok());

    // 4. Load MOD -> Export to XM
    auto mod_song = tracker::mod::ModReader::load_from_memory(mod_bytes_res.value().data(), mod_bytes_res.value().size()).value();
    auto roundtrip_xm_res = tracker::xm::XmWriter::save_to_memory(mod_song);
    REQUIRE(roundtrip_xm_res.is_ok());

    // 5. Final Load from XM and assert data
    auto final_song = tracker::xm::XmReader::load_from_memory(roundtrip_xm_res.value().data(), roundtrip_xm_res.value().size()).value();
    REQUIRE_EQ(final_song.name, orig.name);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(final_song.instruments[0].samples[0].data8.size(), s.data8.size());
}
```

Update `tests/CMakeLists.txt` to include `test_mod_roundtrip.cpp` and `test_cross_format.cpp`.

- [ ] **Step 2: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 64 tests passing.

- [ ] **Step 3: Commit Task 5**

```bash
git add tests/ CMakeLists.txt
git commit -m "test: add MOD round-trip and cross-format conversion verification tests"
```
