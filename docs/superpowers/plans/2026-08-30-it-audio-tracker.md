# Impulse Tracker (.IT) Reader & Writer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the `audio_tracker` C++17 library to read and write Impulse Tracker (`.it`) files (64 channels, 120-note keymaps, 25-node envelopes, NNAs, IT 2.14/2.15 sample decompression) with zero external dependencies, embedded microcontroller compatibility (`-fno-exceptions`, `-fno-rtti`), and full tri-format cross-conversion (`.it` $\leftrightarrow$ `.xm` $\leftrightarrow$ `.mod`).

**Architecture:** Extend `tracker::model.hpp` with IT synthesis/sequencing attributes, implement the IT 2.14/2.15 bitstream sample decompressor in `tracker::it`, build the 64-channel dynamic row mask pattern codec, and implement `ItReader` and `ItWriter` targeting the unified `tracker::Song` data model.

**Tech Stack:** C++17, CMake 3.16+, `-fno-exceptions`, `-fno-rtti`, Little-Endian binary I/O, custom zero-dependency unit test runner.

**Spec:** [`docs/superpowers/specs/2026-08-30-it-audio-tracker-design.md`](file:///Users/moolet/Development/github/newdigate/audio-tracker/docs/superpowers/specs/2026-08-30-it-audio-tracker-design.md)

## Global Constraints
* Language: C++17 (`cxx_std_17`).
* No external dependencies beyond the standard library (C++ STL containers).
* Zero C++ exceptions (`-fno-exceptions`) and zero RTTI (`-fno-rtti`) compatible.
* Little-Endian binary multi-byte integer serialization.
* Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

---

### Task 1: Core Data Model Extensions (`tracker::model.hpp`)

**Files:**
- Modify: `include/tracker/model.hpp`
- Create: `tests/test_it_model.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `Sample::global_volume`, `Sample::c5_speed`, `Sample::sustain_loop_*`, `Instrument::filename`, `Instrument::nna`, `Instrument::dct`, `Instrument::dca`, `Instrument::pitch_envelope`, `Instrument::keyboard_map`, `Song::message`, `Song::channel_volume`, `Song::channel_panning`.

- [ ] **Step 1: Write failing tests for extended data model**

`tests/test_it_model.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/model.hpp>

TEST_CASE(Model_ItExtensions) {
    tracker::Sample sample;
    REQUIRE_EQ(sample.global_volume, 64);
    REQUIRE_EQ(sample.c5_speed, 8363);
    REQUIRE(sample.sustain_loop_type == tracker::LoopType::None);
    REQUIRE_EQ(sample.sustain_loop_start, 0);
    REQUIRE_EQ(sample.sustain_loop_length, 0);

    tracker::Instrument inst;
    REQUIRE(inst.nna == tracker::NewNoteAction::Cut);
    REQUIRE(inst.dct == tracker::DuplicateCheckType::Off);
    REQUIRE(inst.dca == tracker::DuplicateCheckAction::Cut);
    REQUIRE_EQ(inst.global_volume, 128);
    REQUIRE_EQ(inst.default_panning, 128);
    REQUIRE_EQ(inst.keyboard_map.size(), 120);

    // Initial keyboard mapping defaults: note matches index, sample 0
    for (size_t i = 0; i < 120; ++i) {
        REQUIRE_EQ(inst.keyboard_map[i].note, static_cast<uint8_t>(i));
        REQUIRE_EQ(inst.keyboard_map[i].sample, 0);
    }

    tracker::Song song;
    REQUIRE_EQ(song.num_channels, 4); // Default 4 channels
    REQUIRE_EQ(song.global_volume, 128);
    REQUIRE_EQ(song.mix_volume, 48);
    REQUIRE_EQ(song.pan_separation, 128);
    REQUIRE_EQ(song.channel_volume.size(), 64);
    REQUIRE_EQ(song.channel_panning.size(), 64);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing fields in `model.hpp`)

- [ ] **Step 3: Implement data model extensions**

Update `include/tracker/model.hpp` with the new enums (`NewNoteAction`, `DuplicateCheckType`, `DuplicateCheckAction`, `KeyboardNode`) and members on `Sample`, `Instrument`, and `Song`.

Update `tests/CMakeLists.txt` to add `test_it_model.cpp`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 88 tests passing.

- [ ] **Step 5: Commit Task 1**

```bash
git add include/ src/ tests/
git commit -m "feat: extend unified tracker data model for Impulse Tracker features"
```

---

### Task 2: IT 2.14 / 2.15 Sample Decompression Engine

**Files:**
- Create: `include/tracker/it/it_types.hpp`
- Create: `include/tracker/it/it_compression.hpp`
- Create: `src/it/it_compression.cpp`
- Create: `tests/test_it_compression.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::it::decompress_it_sample_8`, `tracker::it::decompress_it_sample_16`.

- [ ] **Step 1: Write failing tests for IT sample decompression**

`tests/test_it_compression.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/it/it_compression.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(ItCompression_8BitEmptyAndSingle) {
    std::vector<int8_t> out;
    std::vector<uint8_t> empty_data;
    tracker::io::MemoryInputStream stream(empty_data.data(), empty_data.size());
    auto status = tracker::it::decompress_it_sample_8(stream, out, 0, false, false);
    REQUIRE(status.is_ok());
    REQUIRE(out.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `it_compression.hpp` and `it_compression.cpp`)

- [ ] **Step 3: Implement IT bitstream decompressor**

`include/tracker/it/it_types.hpp`: Format constants, magic IDs (`"IMPM"`, `"IMPI"`, `"IMPS"`), `ItCommand` enum.
`include/tracker/it/it_compression.hpp`: Prototypes for 8-bit and 16-bit IT 2.14/2.15 decompressors.
`src/it/it_compression.cpp`: Implementation of LSB-first bit reader, adaptive bit-width tracker, and single/double delta integrator.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 90 tests passing.

- [ ] **Step 5: Commit Task 2**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement IT 2.14 and IT 2.15 sample bitstream decompressor"
```

---

### Task 3: IT Pattern Packing & Unpacking Codec

**Files:**
- Create: `include/tracker/it/it_pattern_codec.hpp`
- Create: `src/it/it_pattern_codec.cpp`
- Create: `tests/test_it_pattern_codec.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::it::unpack_pattern`, `tracker::it::pack_pattern`.

- [ ] **Step 1: Write failing tests for IT pattern codec**

`tests/test_it_pattern_codec.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(ItPatternCodec_RoundTrip) {
    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 64, Effect SetSpeed 6
    auto& c = pat.get_cell(0, 0);
    c.note = 49;
    c.instrument = 1;
    c.volume = 0x50; // Vol 64
    c.effect_type = 1; // SetSpeed
    c.effect_param = 6;

    // Row 1, Ch 0: Reuse note & inst, new volume
    auto& c2 = pat.get_cell(1, 0);
    c2.note = 49;
    c2.instrument = 1;
    c2.volume = 0x48; // Vol 56

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 64, 4);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).volume, 0x50);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_type, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_param, 6);

    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).volume, 0x48);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `it_pattern_codec.hpp` and `it_pattern_codec.cpp`)

- [ ] **Step 3: Implement IT pattern codec**

`include/tracker/it/it_pattern_codec.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>

namespace tracker::it {

Status unpack_pattern(io::InputStream& stream, Pattern& out_pat, uint16_t num_rows, uint16_t num_channels);
Status pack_pattern(const Pattern& pat, io::OutputStream& stream);

} // namespace tracker::it
```

`src/it/it_pattern_codec.cpp`:
Implement row packing loop tracking previous note, instrument, volume, effect per channel, writing channel variable (bits 0..6: ch index, bit 7: new mask), mask variable, and values.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 93 tests passing.

- [ ] **Step 5: Commit Task 3**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Impulse Tracker 64-channel pattern packing and unpacking codec"
```

---

### Task 4: IT Reader (`tracker::it::ItReader`)

**Files:**
- Create: `include/tracker/it/it_reader.hpp`
- Create: `src/it/it_reader.cpp`
- Create: `tests/test_it_reader.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::it::ItReader`.

- [ ] **Step 1: Write failing tests for ItReader**

`tests/test_it_reader.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/it/it_reader.hpp>
#include <tracker/io/memory_stream.hpp>

TEST_CASE(ItReader_InvalidHeader) {
    std::vector<uint8_t> short_data(50, 0);
    auto res = tracker::it::ItReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `it_reader.hpp` and `it_reader.cpp`)

- [ ] **Step 3: Implement IT Reader**

`include/tracker/it/it_reader.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <string>

namespace tracker::it {

class ItReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

} // namespace tracker::it
```

`src/it/it_reader.cpp`:
Implement full parsing of `'IMPM'` header, offset pointer arrays, song message, `'IMPI'` instrument headers (25-node envelopes, keyboard maps, NNA, DCT), `'IMPS'` sample headers (sustain loops, C5 speeds, vibrato), pattern streams, and uncompressed / IT compressed audio streams.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 98 tests passing.

- [ ] **Step 5: Commit Task 4**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Impulse Tracker (.IT) file reader"
```

---

### Task 5: IT Writer (`tracker::it::ItWriter`)

**Files:**
- Create: `include/tracker/it/it_writer.hpp`
- Create: `src/it/it_writer.cpp`
- Create: `tests/test_it_writer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `tracker::it::ItWriter`.

- [ ] **Step 1: Write failing tests for ItWriter**

`tests/test_it_writer.cpp`:
```cpp
#include "test_main.hpp"
#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_reader.hpp>

TEST_CASE(ItWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test IT Song";
    song.num_channels = 8;
    song.order_table = {0};
    song.patterns.emplace_back(64, 8);

    auto res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Test IT Song");
    REQUIRE_EQ(load_res.value().num_channels, 8);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build`
Expected: FAIL (missing `it_writer.hpp` and `it_writer.cpp`)

- [ ] **Step 3: Implement IT Writer**

`include/tracker/it/it_writer.hpp`:
```cpp
#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <vector>
#include <string>

namespace tracker::it {

class ItWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::it
```

`src/it/it_writer.cpp`:
Implement offset calculation, writing `'IMPM'` header, order list, pointer arrays (`insnum`, `smpnum`, `patnum`), message block, `'IMPI'` instruments, `'IMPS'` samples, packed patterns, and uncompressed 8/16-bit PCM audio streams.

Update `CMakeLists.txt` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 104 tests passing.

- [ ] **Step 5: Commit Task 5**

```bash
git add include/ src/ tests/ CMakeLists.txt
git commit -m "feat: implement Impulse Tracker (.IT) file writer"
```

---

### Task 6: End-to-End IT Round-Trip & Tri-Format Cross-Conversion

**Files:**
- Create: `tests/test_it_roundtrip.cpp`
- Create: `tests/test_cross_format_it.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `tracker::it::ItWriter`, `tracker::it::ItReader`, `tracker::xm::XmWriter`, `tracker::xm::XmReader`, `tracker::mod::ModWriter`, `tracker::mod::ModReader`.

- [ ] **Step 1: Write comprehensive round-trip and tri-format cross-conversion tests**

`tests/test_it_roundtrip.cpp`:
Full IT song verification (64 channels, NNAs, 120-note keymaps, 25-node volume/panning/pitch envelopes, sustain loops, C5 frequency, song messages).

`tests/test_cross_format_it.cpp`:
Tri-format bidirectional conversion verification:
- `IT` $\rightarrow$ `XM` $\rightarrow$ `MOD` $\rightarrow$ `IT`
- `XM` $\rightarrow$ `IT` $\rightarrow$ `XM`
- `MOD` $\rightarrow$ `IT` $\rightarrow$ `MOD`

Update `tests/CMakeLists.txt`.

- [ ] **Step 2: Build and run test suite**

Run: `cmake --build build && ./build/tests/audio_tracker_tests`
Expected: PASS with 110+ tests passing.

- [ ] **Step 3: Commit Task 6**

```bash
git add tests/ CMakeLists.txt
git commit -m "test: add full IT round-trip and tri-format cross-conversion verification tests"
```
