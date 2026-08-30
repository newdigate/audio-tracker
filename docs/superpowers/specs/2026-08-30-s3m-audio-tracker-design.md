# Scream Tracker 3 (.S3M) Reader & Writer C++ Library Design

## Overview
This document specifies the architecture and technical design for adding **Scream Tracker 3 Module (`.s3m`)** file reading and writing capabilities to the `audio_tracker` C++17 library.

Scream Tracker 3 is one of the defining PC tracker formats, featuring 16-byte paragraph pointers (parapointers), up to 32 channels, unsigned 8-bit PCM audio, C-5 frequency playback rates (`c5_speed`), and dynamic row event packing.

This addition integrates seamlessly into the library's unified `tracker::Song` data model, preserves zero external dependencies, complies with `-fno-exceptions` and `-fno-rtti` on modern microcontrollers (Teensy 4.x / i.MX RT1176), and provides 4-way cross-conversion across all major tracker formats (**FastTracker II `.xm`**, **ProTracker `.mod`**, **Impulse Tracker `.it`**, and **Scream Tracker 3 `.s3m`**).

---

## 1. Requirements and Constraints

### 1.1 Platform & Architecture Targets
* **Target Platforms:** Embedded microcontrollers (Teensy 4.x / MIMXRT1176 ARM Cortex-M7) and desktop operating systems (macOS, Linux, Windows).
* **Endianness:** Little-Endian multi-byte integers (`read_u16_le`, `read_u32_le`, `write_u16_le`, `write_u32_le`).
* **Zero Dependencies:** C++17 STL containers only.
* **Exceptions & RTTI:** Full compatibility with `-fno-exceptions` and `-fno-rtti`.
* **Error Handling:** Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

### 1.2 Functional Scope
* **Channel Support:** 1 to 32 PCM audio channels.
* **Sample Formats:** Unsigned 8-bit PCM (standard ST3 `ffi == 2`) and signed 8-bit PCM (`ffi == 1`), with forward loops and C5 middle frequencies.
* **Pattern Rows:** 64 rows per pattern packed via channel control bitmasks.
* **Parapointers:** 16-byte paragraph offset calculation and alignment padding.
* **4-Way Cross-Format Conversion:** Full bidirectional conversion across `.s3m`, `.xm`, `.mod`, and `.it`.

---

## 2. S3M Binary File Layout & Parapointers

Scream Tracker 3 files organize internal structures using **16-byte paragraph pointers**:
$$\text{File Byte Offset} = \text{Parapointer} \times 16$$

```
0x0000   96 bytes   "SCRM" Main Header
0x0060   OrdNum     Order Sequence Table (Values 0..PatNum-1, 255=End, 254=Skip)
Offset   InsNum * 2 Sample Parapointer Array (uint16_t[InsNum])
Offset   PatNum * 2 Pattern Parapointer Array (uint16_t[PatNum])
Offset   32 bytes   Optional Custom Panning Table (if default_panning_present == 0xFC)
-------------------------------------------------------------------------------
Aligned  80 bytes   "SCRS" Sample Headers (InsNum records aligned to 16-byte boundaries)
Aligned  ... bytes  Pattern Data Blocks (PatNum records aligned to 16-byte boundaries)
Aligned  ... bytes  Raw Sample PCM Data (Unsigned 8-bit PCM aligned to 16-byte boundaries)
```

### 2.1 Main Header (`"SCRM"`, 96 Bytes at Offset 0x0000)
```
Offset   Length   Type        Field Name       Description
-------------------------------------------------------------------------------
0x0000   28       char[28]    song_title       ASCII title (null/space padded)
0x001C   1        uint8       dos_eof          0x1A (DOS EOF marker)
0x001D   1        uint8       file_type        0x10 (ST3 Module)
0x001E   2        uint16      reserved1        0x0000
0x0020   2        uint16      ordnum           Number of orders
0x0022   2        uint16      insnum           Number of instruments / samples (1..99)
0x0024   2        uint16      patnum           Number of patterns (0..100)
0x0026   2        uint16      flags            Bit 0: ST2 Vibrato, Bit 2: Amiga slides
0x0028   2        uint16      cwt_vwt          Tracker version (e.g. 0x1320 for ST3.20)
0x002A   2        uint16      ffi              Format info (1 = signed, 2 = unsigned PCM)
0x002C   4        char[4]     magic            "SCRM" (0x4D524353)
0x0030   1        uint8       global_volume    Global volume (0..64)
0x0031   1        uint8       initial_speed    Initial speed in ticks/row (1..255, default 6)
0x0032   1        uint8       initial_tempo    Initial BPM (32..255, default 125)
0x0033   1        uint8       master_volume    Master volume (0..127, bit 7: stereo)
0x0034   1        uint8       ultraclick       Ultra-click removal channels
0x0035   1        uint8       default_pan_tag  0xFC if 32-byte panning table follows
0x0036   8        uint8[8]    reserved2        Reserved
0x003E   2        uint16      special_ptr      Parapointer to special message/data
0x0040   32       uint8[32]   channel_settings Channel routing table (0..15: Left/Right, 0xFF: Muted)
```

### 2.2 Sample Header (`"SCRS"`, 80 Bytes)
Target of `ins_parapointers[i] * 16`:
```
Offset   Length   Type        Field Name       Description
-------------------------------------------------------------------------------
0x00     1        uint8       sample_type      1 = Digital PCM Sample (2/3 = AdLib FM)
0x01     12       char[12]    dos_filename     DOS 8.3 filename
0x0D     1        uint8       memseg_hi        High 8 bits of 24-bit PCM parapointer
0x0E     2        uint16      memseg_lo        Low 16 bits of 24-bit PCM parapointer
                                               (PCM Offset = ((memseg_hi << 16) | memseg_lo) * 16)
0x10     4        uint32      length           Sample length in bytes
0x14     4        uint32      loop_start       Loop start byte offset
0x18     4        uint32      loop_end         Loop end byte offset
0x1C     1        uint8       volume           Default volume (0..64)
0x1D     1        uint8       dsk              Disk drive
0x1E     1        uint8       pack             Packing type (0 = uncompressed, 1 = DP30)
0x1F     1        uint8       flags            Bit 0: Loop on, Bit 1: Stereo, Bit 2: 16-bit
0x20     4        uint32      c5_speed         C-5 middle playback frequency in Hz
0x24     12       uint8[12]   reserved         Internal player variables
0x30     28       char[28]    sample_name      Sample name (null-padded)
0x4C     4        char[4]     magic            "SCRS" (0x53524353)
```

---

## 3. Pattern Row Codec & Effect Translation

### 3.1 Pattern Packing Scheme
Each S3M pattern begins with a 2-byte length field (`uint16_t length` = packed byte size + 2).
It contains 64 rows packed sequentially, where each row consists of zero or more channel events terminated by a **`0x00` (End of Row)** byte.

```
[Channel Control Byte] (1 byte)
  Bits 0..4: Channel index (0..31)
  Bit 5 (0x20): Note byte + Instrument byte follow
  Bit 6 (0x40): Volume byte follows (0..64)
  Bit 7 (0x80): Effect Command byte (1..26) + Effect Parameter byte follow
  (If Channel Control Byte == 0x00 -> End of current row)
```

**Note Byte Decoding (`0x00..0x9B`):**
$$\text{Octave} = \text{byte} \gg 4, \quad \text{Semitone} = \text{byte} \& 0x0F$$
$$\text{Note Index} = \text{Octave} \times 12 + \text{Semitone} + 1 \quad (1 \dots 120)$$
$$\text{Special Value } 0xFE = \text{Key Off } (\text{note } 97), \quad 0xFF = \text{No Note}$$

### 3.2 S3M Effect Commands (`1..26` $\leftrightarrow$ `'A'..'Z'`)
```cpp
namespace tracker::s3m {

enum class S3mCommand : uint8_t {
    None = 0,
    SetSpeed = 1,          // A: Set Speed (ticks/row)
    JumpToOrder = 2,       // B: Order Jump
    PatternBreak = 3,      // C: Pattern Break
    VolumeSlide = 4,       // D: Volume Slide
    PortamentoDown = 5,    // E: Portamento Down
    PortamentoUp = 6,      // F: Portamento Up
    TonePortamento = 7,    // G: Tone Portamento (Slide to Note)
    Vibrato = 8,           // H: Vibrato
    Tremor = 9,            // I: Tremor
    Arpeggio = 10,         // J: Arpeggio
    DualVibratoVol = 11,   // K: Vibrato + Volume Slide
    DualTonePortaVol = 12, // L: Tone Portamento + Volume Slide
    SampleOffset = 15,     // O: Sample Offset
    RetriggerNote = 17,    // Q: Retrigger Note
    Tremolo = 18,          // R: Tremolo
    Extended = 19,         // S: Extended Sub-commands
    SetTempo = 20,         // T: Set Tempo (BPM)
    FineVibrato = 21,      // U: Fine Vibrato
    GlobalVolume = 22,     // V: Set Global Volume (0..64)
    SetPanning = 24        // X: Set Panning (0..128 / 0..255)
};

} // namespace tracker::s3m
```

---

## 4. Sample Audio Format & PCM Conversion

### 4.1 Unsigned vs Signed PCM Conversion
* **Reading (ST3 Unsigned `ffi == 2`)**:
  $$x_{\text{signed}} = \text{static\_cast<int8\_t>}(x_{\text{unsigned}} \oplus 0x80)$$
* **Writing (ST3 Unsigned `ffi = 2`)**:
  $$x_{\text{unsigned}} = \text{static\_cast<uint8\_t>}(x_{\text{signed}} \oplus 0x80)$$
* If 16-bit audio in `tracker::Song`: downsample $x_{16} \gg 8$ to unsigned 8-bit.

---

## 5. `S3mReader` & `S3mWriter` Interface

```cpp
namespace tracker::s3m {

class S3mReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

class S3mWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::s3m
```

---

## 6. Verification and Testing Strategy
1. **`test_s3m_pattern_codec`**: Validates 32-channel packing/unpacking, channel control flags, note octave/semitone packing, volume/effect encoding, and end-of-row markers.
2. **`test_s3m_reader`**: Validates `"SCRM"` header parsing, 16-byte parapointer resolution, `"SCRS"` sample headers, C5 speeds, and unsigned-to-signed PCM conversion.
3. **`test_s3m_writer`**: Validates 16-byte paragraph alignment padding, sample/pattern parapointer tables, and signed-to-unsigned PCM conversion.
4. **`test_s3m_roundtrip`**: Validates 100% full song round-trip fidelity in memory and filesystem streams.
5. **`test_cross_format_s3m`**: Validates 4-way cross-conversion across all major formats:
   $$\text{S3M} \longleftrightarrow \text{XM} \longleftrightarrow \text{MOD} \longleftrightarrow \text{IT}$$
