# ProTracker / FastTracker (.MOD) Reader & Writer C++ Library Design

## Overview
This document specifies the architecture and technical design for adding **Soundtracker / ProTracker / FastTracker Module (`.mod`)** file reading and writing capabilities to the `audio_tracker` C++17 library.

The `.mod` format originated on the Commodore Amiga (Motorola 68000 Big-Endian architecture). This extension adds Big-Endian stream support, Amiga PAL period lookup tables, 4-byte packed note cell codecs, and multi-channel signature detection to seamlessly integrate with the library's existing unified `tracker::Song` model, zero-dependency philosophy, and microcontroller compatibility (`-fno-exceptions`, `-fno-rtti`).

---

## 1. Requirements and Constraints

### 1.1 Platform & Architecture Targets
* **Target Platforms:** Embedded microcontrollers (Teensy 4.x / MIMXRT1176) and desktop OSs (macOS, Linux, Windows).
* **Architecture:** ARM Cortex-M (Little-Endian) and x86_64/AArch64.
* **Endianness Handling:** Explicit bit-shift operations for Big-Endian Motorola 68000 multi-byte fields (`read_u16_be`, `write_u16_be`, `read_u32_be`, `write_u32_be`).
* **Zero Dependencies:** Standard C++ library only.
* **Exceptions & RTTI:** Full compatibility with `-fno-exceptions` and `-fno-rtti`.

### 1.2 Functional Scope
* **Supported Variants:** Standard 31-sample MODs with 4, 6, 8, 10, 12, 16, 24, and 32 channels.
* **Signature Recognition:**
  * 4-channel: `"M.K."`, `"M!K!"`, `"4CHN"`, `"FLT4"`
  * 6-channel: `"6CHN"`
  * 8-channel: `"8CHN"`, `"OCTA"`, `"CD81"`, `"FLT8"`
  * Generic $N$-channel: `"xCHN"`, `"xxCN"`
* **Amiga Period Resolution:** 5-octave PAL period table (60 notes, C-0 to B-5) with closest-match search for detuned periods.
* **Raw PCM Audio:** 8-bit signed linear PCM (direct streaming, no delta encoding).
* **Bidirectional Cross-Format Conversion:** Full interoperability between `.xm` and `.mod` via the shared `tracker::Song` model.

---

## 2. Big-Endian Stream I/O Extensions

In `tracker::io::InputStream`:
```cpp
uint16_t read_u16_be();
int16_t  read_i16_be();
uint32_t read_u32_be();
```

In `tracker::io::OutputStream`:
```cpp
void write_u16_be(uint16_t val);
void write_i16_be(int16_t val);
void write_u32_be(uint32_t val);
```

Implementation uses host-independent byte shifts:
$$\text{u16\_be} = (b[0] \ll 8) \mid b[1]$$
$$\text{u32\_be} = (b[0] \ll 24) \mid (b[1] \ll 16) \mid (b[2] \ll 8) \mid b[3]$$

---

## 3. Standard 31-Sample MOD Binary File Layout

A standard 31-sample `.mod` file has a fixed 1084-byte header:

```
Offset   Length   Type     Field Name      Description
-------------------------------------------------------------------------------
0x0000   20       char[20] song_title      Song title (ASCII, null-padded)
0x0014   30       struct   sample_1        Sample 1 header
0x0032   30       struct   sample_2        Sample 2 header
...      ...      ...      ...             ...
0x0398   30       struct   sample_31       Sample 31 header
0x03B6   1        uint8    song_length     Number of orders to play (1..128)
0x03B7   1        uint8    restart_pos     Restart position (typically 127)
0x03B8   128      uint8[128] order_table   Pattern sequence table (0..127)
0x0438   4        char[4]  signature_tag   Format tag ("M.K.", "4CHN", "8CHN", etc.)
-------------------------------------------------------------------------------
0x043C   ...      bytes    patterns        Pattern data (num_patterns * 64 * channels * 4 bytes)
Follows  ...      bytes    samples         Sample audio data (consecutive 8-bit signed PCM)
```

### 30-Byte Sample Header Format
```
Offset   Length   Type     Description
-------------------------------------------------------------------------------
+0x00    22       char[22] Sample name (ASCII, space/null-padded)
+0x16    2        uint16   Sample length in 16-bit words (bytes = length * 2)
+0x18    1        uint8    Finetune (signed 4-bit nibble in lower bits 0..3)
+0x19    1        uint8    Default volume (0..64)
+0x1A    2        uint16   Loop start in 16-bit words (bytes = loop_start * 2)
+0x1C    2        uint16   Loop length in 16-bit words (bytes = loop_length * 2)
```
*Note on loop length:* In ProTracker MOD files, if `loop_length <= 1` (i.e. $\le 2$ bytes), loop playback is disabled.

---

## 4. Amiga Period Tables & 4-Byte Cell Codec

### 4.1 4-Byte Note Cell Packing
Each cell in every row is strictly 4 bytes:
```
Byte 0:  [ S7 S6 S5 S4 | P11 P10 P9  P8  ]   (Upper 4 bits of Sample + Upper 4 bits of Period)
Byte 1:  [ P7 P6 P5 P4 | P3  P2  P1  P0  ]   (Lower 8 bits of Period)
Byte 2:  [ S3 S2 S1 S0 | E3  E2  E1  E0  ]   (Lower 4 bits of Sample + 4-bit Effect Type)
Byte 3:  [ X7 X6 X5 X4 | X3  X2  X1  X0  ]   (8-bit Effect Parameter)
```

* **Sample Number (1..31):** `(b[0] & 0xF0) | (b[2] >> 4)`
* **Amiga Period (12-bit):** `((b[0] & 0x0F) << 8) | b[1]`
* **Effect Type (0x0..0xF):** `b[2] & 0x0F`
* **Effect Parameter (0x00..0xFF):** `b[3]`

### 4.2 5-Octave Amiga PAL Period Table
Note `1` = C-0, Note `13` = C-1 (ProTracker base), Note `49` = C-4:

```cpp
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
```

---

## 5. `ModReader` & `ModWriter` Specifications

### 5.1 `tracker::mod::ModReader`
* Parse header, 31 sample headers, and 128-byte order table.
* Read format tag at offset 1080 to determine channel count ($4, 6, 8, \dots, 32$).
* Calculate `num_patterns` from the maximum pattern index referenced in `order_table[0..song_length-1]`.
* Unpack 4-byte cells into `tracker::Cell` grid using `period_to_note()`.
* Read 31 consecutive signed 8-bit PCM sample buffers into `tracker::Sample::data8`.
* Set `Song::linear_frequency = false` (Amiga mode), `default_speed = 6`, `default_bpm = 125`.

### 5.2 `tracker::mod::ModWriter`
* Serialize 20-byte title, 31 sample headers with lengths/loops converted to 16-bit words.
* Write 128-byte order table and appropriate signature tag (`"M.K."`, `"6CHN"`, `"8CHN"`, etc.).
* Pack cells into 4-byte events using `note_to_period()`.
* Downsample any 16-bit audio samples to 8-bit signed PCM (`sample16 >> 8`) and write raw bytes.

---

## 6. Verification and Testing Strategy
1. **`test_stream_be`**: Validates `read_u16_be`, `write_u16_be`, `read_u32_be`, `write_u32_be`.
2. **`test_mod_cell`**: Validates period table lookups, closest-match period snapping, and 4-byte cell packing/unpacking.
3. **`test_mod_reader`**: Validates signature parsing, channel counts, pattern extraction, and sample header calculations.
4. **`test_mod_writer`**: Validates MOD serialization, signature generation, and sample word-length encoding.
5. **`test_mod_roundtrip`**: Programmatic MOD song construction, serialization, deserialization, and field-by-field verification.
6. **`test_cross_format`**: Bidirectional conversion between `.xm` and `.mod` verifying notes, channels, patterns, and audio buffers.
