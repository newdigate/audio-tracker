# Scream Tracker 3 (.S3M) Reader & Writer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the `audio_tracker` C++17 library to read and write Scream Tracker 3 (`.s3m`) files (32 channels, 16-byte parapointers, unsigned 8-bit PCM audio, C5 frequency speeds) with zero external dependencies, embedded microcontroller compatibility (`-fno-exceptions`, `-fno-rtti`), and full 4-way cross-conversion across all major formats (`.s3m` $\leftrightarrow$ `.xm` $\leftrightarrow$ `.mod` $\leftrightarrow$ `.it`).

**Architecture:** Add S3M types, constants, and parapointer utilities in `tracker::s3m`, implement the 32-channel row packing/unpacking pattern codec, and build `S3mReader` and `S3mWriter` interfacing directly with the unified `tracker::Song` data model.

**Tech Stack:** C++17, CMake 3.16+, `-fno-exceptions`, `-fno-rtti`, Little-Endian binary I/O, custom zero-dependency unit test runner.

**Spec:** [`docs/superpowers/specs/2026-08-30-s3m-audio-tracker-design.md`](file:///Users/moolet/Development/github/newdigate/audio-tracker/docs/superpowers/specs/2026-08-30-s3m-audio-tracker-design.md)

## Global Constraints
* Language: C++17 (`cxx_std_17`).
* No external dependencies beyond the standard library (C++ STL containers).
* Zero C++ exceptions (`-fno-exceptions`) and zero RTTI (`-fno-rtti`) compatible.
* Little-Endian binary multi-byte integer serialization.
* Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

---

### Task 1: S3M Types, Constants & Parapointer Helpers

**Files:**
- Create: `include/tracker/s3m/s3m_types.hpp`
- Create: `src/s3m/s3m_types.cpp`
- Create: `tests/test_s3m_types.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::s3m::S3M_MAGIC_SCRM`, `tracker::s3m::S3M_MAGIC_SCRS`, `tracker::s3m::S3mCommand`, `tracker::s3m::parapointer_to_offset`, `tracker::s3m::offset_to_parapointer`, `tracker::s3m::note_to_s3m_byte`, `tracker::s3m::s3m_byte_to_note`.

- [ ] **Step 1: Write failing tests for S3M types & helpers**

`tests/test_s3m_types.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/s3m/s3m_types.hpp>

TEST_CASE(S3mTypes_ParapointerHelpers) {
    REQUIRE_EQ(tracker::s3m::parapointer_to_offset(0x0010), 256);
    REQUIRE_EQ(tracker::s3m::offset_to_parapointer(256), 0x0010);
    // Alignment to paragraph boundary (16 bytes)
    REQUIRE_EQ(tracker::s3m::align_paragraph_offset(250), 256);
    REQUIRE_EQ(tracker::s3m::align_paragraph_offset(256), 256);
}

TEST_CASE(S3mTypes_NoteConversion) {
    // Note 1 = C-0 (Octave 0, Semitone 0 -> 0x00)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(1), 0x00);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x00), 1);

    // Note 13 = C-1 (Octave 1, Semitone 0 -> 0x10)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(13), 0x10);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x10), 13);

    // Note 49 = C-4 (Octave 4, Semitone 0 -> 0x40)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(49), 0x40);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x40), 49);

    // Key-off (Note 97 / 0x61) -> 0xFE
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(97), 0xFE);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0xFE), 97);

    // Empty note -> 0xFF
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(0), 0xFF);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0xFF), 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `s3m_types.hpp`)

- [ ] **Step 3: Implement S3M types & helpers**

`include/tracker/s3m/s3m_types.hpp`:
Format constants, magic tags (`"SCRM"`, `"SCRS"`), `S3mCommand` enum, inline helpers for parapointers and note conversions.

`src/s3m/s3m_types.cpp`:
Implement non-inline functions if any.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 148 tests passing.

- [ ] **Step 5: Commit Task 1**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: add Scream Tracker 3 format definitions and parapointer helpers"
```

---

### Task 2: S3M Pattern Packing & Unpacking Codec

**Files:**
- Create: `include/tracker/s3m/s3m_pattern_codec.hpp`
- Create: `src/s3m/s3m_pattern_codec.cpp`
- Create: `tests/test_s3m_pattern_codec.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::s3m::unpack_pattern`, `tracker::s3m::pack_pattern`.

- [ ] **Step 1: Write failing tests for S3M pattern codec**

`tests/test_s3m_pattern_codec.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(S3mPatternCodec_RoundTrip) {
    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 64, Effect SetSpeed 6
    auto& c = pat.get_cell(0, 0);
    c.note = 49;
    c.instrument = 1;
    c.volume = 0x50; // Vol 64
    c.effect_type = 1; // SetSpeed
    c.effect_param = 6;

    // Row 10, Ch 2: Note 25 (C-2), Inst 2, Vol 32
    auto& c2 = pat.get_cell(10, 2);
    c2.note = 25;
    c2.instrument = 2;
    c2.volume = 0x30; // Vol 32

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 4);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).volume, 0x50);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_type, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_param, 6);

    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).note, 25);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).instrument, 2);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).volume, 0x30);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `s3m_pattern_codec.hpp` and `s3m_pattern_codec.cpp`)

- [ ] **Step 3: Implement S3M pattern codec**

`include/tracker/s3m/s3m_pattern_codec.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>

namespace tracker::s3m {

Status unpack_pattern(io::InputStream& stream, Pattern& out_pat, uint16_t num_channels);
Status pack_pattern(const Pattern& pat, io::OutputStream& stream);

} // namespace tracker::s3m
```

`src/s3m/s3m_pattern_codec.cpp`:
Implement S3M 64-row packing loop with `channel_control` byte flags (`0x20` Note/Inst, `0x40` Volume, `0x80` Effect/Param) and `0x00` row end delimiter.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 151 tests passing.

- [ ] **Step 5: Commit Task 2**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Scream Tracker 3 pattern packing and unpacking codec"
```

---

### Task 3: S3M Reader (`tracker::s3m::S3mReader`)

**Files:**
- Create: `include/tracker/s3m/s3m_reader.hpp`
- Create: `src/s3m/s3m_reader.cpp`
- Create: `tests/test_s3m_reader.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::s3m::S3mReader`.

- [ ] **Step 1: Write failing tests for S3mReader**

`tests/test_s3m_reader.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/s3m/s3m_reader.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(S3mReader_InvalidHeader) {
    std::vector<uint8_t> short_data(50, 0);
    auto res = tracker::s3m::S3mReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `s3m_reader.hpp` and `s3m_reader.cpp`)

- [ ] **Step 3: Implement S3M Reader**

`include/tracker/s3m/s3m_reader.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <string>

namespace tracker::s3m {

class S3mReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

} // namespace tracker::s3m
```

`src/s3m/s3m_reader.cpp`:
Implement deserialization of `"SCRM"` header, order table, sample & pattern parapointer tables, optional custom panning table, `"SCRS"` sample headers, C5 speeds, unsigned/signed 8-bit PCM audio, and pattern streams.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 157 tests passing.

- [ ] **Step 5: Commit Task 3**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Scream Tracker 3 (.S3M) file reader"
```

---

### Task 4: S3M Writer (`tracker::s3m::S3mWriter`)

**Files:**
- Create: `include/tracker/s3m/s3m_writer.hpp`
- Create: `src/s3m/s3m_writer.cpp`
- Create: `tests/test_s3m_writer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::s3m::S3mWriter`.

- [ ] **Step 1: Write failing tests for S3mWriter**

`tests/test_s3m_writer.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/s3m/s3m_writer.hpp>
#include <tracker/s3m/s3m_reader.hpp>

TEST_CASE(S3mWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test S3M Song";
    song.num_channels = 8;
    song.order_table = {0};
    song.patterns.emplace_back(64, 8);

    auto res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Test S3M Song");
    REQUIRE_EQ(load_res.value().num_channels, 8);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `s3m_writer.hpp` and `s3m_writer.cpp`)

- [ ] **Step 3: Implement S3M Writer**

`include/tracker/s3m/s3m_writer.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <vector>
#include <string>

namespace tracker::s3m {

class S3mWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::s3m
```

`src/s3m/s3m_writer.cpp`:
Calculate 16-byte paragraph alignment padding, write `"SCRM"` header, order table, sample & pattern parapointer tables, custom 32-channel panning table, `"SCRS"` sample headers, packed 64-row patterns, and unsigned 8-bit PCM audio streams.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 163 tests passing.

- [ ] **Step 5: Commit Task 4**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Scream Tracker 3 (.S3M) file writer"
```

---

### Task 5: End-to-End S3M Round-Trip & 4-Way Cross-Conversion

**Files:**
- Create: `tests/test_s3m_roundtrip.cpp`
- Create: `tests/test_cross_format_s3m.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::s3m::S3mWriter`, `tracker::s3m::S3mReader`, `tracker::xm::XmWriter`, `tracker::xm::XmReader`, `tracker::mod::ModWriter`, `tracker::mod::ModReader`, `tracker::it::ItWriter`, `tracker::it::ItReader`.

- [ ] **Step 1: Write comprehensive S3M round-trip and 4-way cross-conversion tests**

`tests/test_s3m_roundtrip.cpp`:
Full S3M song verification (32 channels, unsigned 8-bit PCM, C5 frequency, loop points, volume column, effects, file I/O).

`tests/test_cross_format_s3m.cpp`:
4-Way bidirectional cross-conversion cycles:
- `S3M` $\rightarrow$ `XM` $\rightarrow$ `MOD` $\rightarrow$ `IT` $\rightarrow$ `S3M`
- `XM` $\rightarrow$ `S3M` $\rightarrow$ `XM`
- `MOD` $\rightarrow$ `S3M` $\rightarrow$ `MOD`
- `IT` $\rightarrow$ `S3M` $\rightarrow$ `IT`

Update `tests/CMakeLists.txt`.

- [ ] **Step 2: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 170+ tests passing.

- [ ] **Step 3: Commit Task 5**

```bash
git add tests/ CMakeLists.txt
git commit -m "test: add full S3M round-trip and 4-way cross-conversion verification tests"
```
