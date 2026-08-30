# Impulse Tracker (.IT) Reader & Writer C++ Library Design

## Overview
This document specifies the architecture and technical design for adding **Impulse Tracker Module (`.it`)** file reading and writing capabilities to the `audio_tracker` C++17 library.

Impulse Tracker is a sophisticated tracker format introducing 64 channels, 120-note keymaps, 25-node envelopes (Volume, Panning, and Pitch/Filter), New Note Actions (NNA), Duplicate Check Types (DCT/DCA), middle-C playback speeds (`c5_speed`), and adaptive bitstream sample compression (IT 2.14 / 2.15).

This addition seamlessly integrates into the unified `tracker::Song` data model, maintains zero external dependencies, complies with `-fno-exceptions` and `-fno-rtti`, and provides bidirectional cross-format interoperability across **FastTracker II (`.xm`)**, **ProTracker (`.mod`)**, and **Impulse Tracker (`.it`)**.

---

## 1. Requirements and Constraints

### 1.1 Platform & Architecture Targets
* **Target Platforms:** Embedded microcontrollers (Teensy 4.x / MIMXRT1176 ARM Cortex-M7) and desktop operating systems (macOS, Linux, Windows).
* **Endianness:** Little-Endian multi-byte integers (`read_u16_le`, `read_u32_le`, `write_u16_le`, `write_u32_le`).
* **Zero Dependencies:** C++17 STL containers only.
* **Exceptions & RTTI:** Zero runtime exceptions (`-fno-exceptions`) and zero RTTI (`-fno-rtti`).
* **Error Handling:** Value-based error handling via `tracker::Status` and `tracker::Result<T>`.

### 1.2 Functional Scope
* **Channel Support:** 1 to 64 channels.
* **Playback Modes:** Full support for both **Instrument Mode** (`.iti` headers with 120-note keymaps, envelopes, NNAs) and **Sample Mode** (direct note-to-sample triggering).
* **Sample Formats:** 8-bit and 16-bit uncompressed PCM (mono and stereo), plus native bitstream decompressor for IT 2.14 and IT 2.15 compressed samples.
* **Envelopes:** Up to 25 nodes for Volume, Panning, and Pitch/Filter envelopes with sustain and loop points.
* **Cross-Format Conversion:** Full bidirectional conversion between `.it`, `.xm`, and `.mod`.

---

## 2. Core Data Model Extensions (`tracker::model.hpp`)

### 2.1 `tracker::Sample` Extensions
```cpp
struct Sample {
    std::string name;
    uint32_t length{0};          // Length in sample frames
    uint32_t loop_start{0};      // Loop start in sample frames
    uint32_t loop_length{0};     // Loop length in sample frames
    uint8_t volume{64};          // Default volume (0..64)
    uint8_t global_volume{64};   // Sample global volume (0..64) [IT]
    int8_t finetune{0};          // Finetune (-128..+127)
    LoopType loop_type{LoopType::None};
    bool is_16bit{false};
    uint8_t panning{128};        // Panning (0..255, 128 = Center)
    int8_t relative_note{0};     // Relative note (-96..+95)
    uint32_t c5_speed{8363};     // C-5 frequency in Hz [IT]

    // Sustain Loops [IT]
    LoopType sustain_loop_type{LoopType::None};
    uint32_t sustain_loop_start{0};
    uint32_t sustain_loop_length{0};

    // Vibrato parameters
    uint8_t vibrato_type{0};
    uint8_t vibrato_sweep{0};
    uint8_t vibrato_depth{0};
    uint8_t vibrato_rate{0};

    std::vector<int8_t> data8;
    std::vector<int16_t> data16;
};
```

### 2.2 `tracker::Instrument` Extensions
```cpp
enum class NewNoteAction : uint8_t { Cut = 0, Continue = 1, NoteOff = 2, NoteFade = 3 };
enum class DuplicateCheckType : uint8_t { Off = 0, Note = 1, Sample = 2, Instrument = 3 };
enum class DuplicateCheckAction : uint8_t { Cut = 0, NoteOff = 1, NoteFade = 2 };

struct KeyboardNode {
    uint8_t note{0};    // Note to play (0..119)
    uint8_t sample{0};  // Sample index (1..99, 0 = none)
};

struct Instrument {
    std::string name;
    std::string filename;        // DOS filename (12 chars) [IT]
    uint8_t type{0};
    NewNoteAction nna{NewNoteAction::Cut};
    DuplicateCheckType dct{DuplicateCheckType::Off};
    DuplicateCheckAction dca{DuplicateCheckAction::Cut};
    uint16_t volume_fadeout{0};
    uint8_t global_volume{128};  // Instrument global volume (0..128) [IT]
    uint8_t default_panning{128};// Default instrument pan (0..255)

    // 120-note keyboard assignment map [IT]
    std::array<KeyboardNode, 120> keyboard_map{};

    Envelope volume_envelope;
    Envelope panning_envelope;
    Envelope pitch_envelope;    // Pitch / Filter envelope [IT]

    std::vector<Sample> samples;
};
```

### 2.3 `tracker::Song` Extensions
```cpp
struct Song {
    std::string name;
    std::string tracker_name{"Impulse Tracker"};
    std::string message;         // Embedded song message / comments [IT]
    uint16_t version{0x0214};
    uint16_t restart_position{0};
    uint16_t num_channels{64};   // Up to 64 channels in IT
    bool linear_frequency{true};
    uint16_t default_speed{6};
    uint16_t default_bpm{125};
    uint8_t global_volume{128};  // 0..128 [IT]
    uint8_t mix_volume{48};      // 0..128 [IT]
    uint8_t pan_separation{128}; // 0..128 [IT]

    std::array<uint8_t, 64> channel_volume{};  // 0..64 per channel [IT]
    std::array<uint8_t, 64> channel_panning{}; // 0..64 per channel [IT]

    std::vector<uint8_t> order_table;
    std::vector<Pattern> patterns;
    std::vector<Instrument> instruments;
};
```

---

## 3. Impulse Tracker Binary File Layout

An Impulse Tracker (`.it`) file is structured as follows:

```
0x0000   192 bytes   'IMPM' Main Song Header
Offset   OrdNum      Order Sequence Table (0..PatNum-1, 255=End)
Offset   InsNum * 4  Instrument Offset Pointer Table (uint32_t[InsNum])
Offset   SmpNum * 4  Sample Offset Pointer Table (uint32_t[SmpNum])
Offset   PatNum * 4  Pattern Offset Pointer Table (uint32_t[PatNum])
Offset   ...         Embedded Song Message (if Special & 0x01)
Offset   ...         'IMPI' Instrument Headers (InsNum * 554 bytes)
Offset   ...         'IMPS' Sample Headers (SmpNum * 80 bytes)
Offset   ...         Pattern Data Blocks (Packed variable-row channels)
Offset   ...         Sample Audio Streams (8-bit / 16-bit PCM / Compressed)
```

### 3.1 Main Song Header (`'IMPM'`, 192 Bytes)
```
Offset   Length   Type        Field Name       Description
-------------------------------------------------------------------------------
0x0000   4        char[4]     magic            "IMPM" (0x4D504D49)
0x0004   26       char[26]    song_name        ASCII title (null-padded)
0x001E   2        uint16      phighlight       Pattern row/beat highlight
0x0020   2        uint16      ordnum           Number of orders
0x0022   2        uint16      insnum           Number of instruments
0x0024   2        uint16      smpnum           Number of samples
0x0026   2        uint16      patnum           Number of patterns
0x0028   2        uint16      cwt_vwt          Tracker version (e.g. 0x0214)
0x002A   2        uint16      cmwt             Compatible version (e.g. 0x0200)
0x002C   2        uint16      flags            Bit 0: Stereo, Bit 2: Use Instruments,
                                               Bit 3: Linear Slides, Bit 4: Old FX
0x002E   2        uint16      special          Bit 0: Embedded message present
0x0030   1        uint8       global_volume    Global volume (0..128)
0x0031   1        uint8       mix_volume       Mixing volume (0..128)
0x0032   1        uint8       initial_speed    Initial ticks per row (1..255)
0x0033   1        uint8       initial_tempo    Initial BPM (32..255)
0x0034   1        uint8       pan_separation   Stereo pan separation (0..128)
0x0035   1        uint8       pitch_wheel      Pitch wheel depth
0x0036   2        uint16      msg_length       Song message length in bytes
0x0038   4        uint32      msg_offset       File offset to song message
0x003C   4        uint32      reserved         Reserved
0x0040   64       uint8[64]   channel_pan      Default channel panning (0..64, bit 7: muted)
0x0080   64       uint8[64]   channel_vol      Default channel volume (0..64)
```

### 3.2 Sample Header (`'IMPS'`, 80 Bytes)
```
Offset   Length   Type        Field Name       Description
-------------------------------------------------------------------------------
0x00     4        char[4]     magic            "IMPS" (0x53504D49)
0x04     12       char[12]    dos_filename     DOS 8.3 filename
0x10     1        uint8       zero             0
0x11     1        uint8       global_vol       Sample global volume (0..64)
0x12     1        uint8       flags            Bit 0: Has sample, Bit 1: 16-bit,
                                               Bit 2: Stereo, Bit 3: Compressed,
                                               Bit 4: Loop On, Bit 5: Sustain Loop,
                                               Bit 6: PingPong Loop, Bit 7: PingPong Sus
0x13     1        uint8       default_vol      Default volume (0..64)
0x14     26       char[26]    sample_name      Sample name (null-padded)
0x2E     1        uint8       convert_flags    Bit 0: Signed PCM, Bit 1: Big-Endian, Bit 2: Delta
0x2F     1        uint8       default_pan      Default pan (0..64, bit 7: enabled)
0x30     4        uint32      length           Sample length in frames
0x34     4        uint32      loop_start       Loop start frame
0x38     4        uint32      loop_end         Loop end frame
0x3C     4        uint32      c5_speed         C-5 frequency in Hz (e.g. 8363)
0x40     4        uint32      sus_loop_start   Sustain loop start frame
0x44     4        uint32      sus_loop_end     Sustain loop end frame
0x48     4        uint32      sample_pointer   File offset to audio data
0x4C     1        uint8       vibrato_speed    Auto-vibrato sweep
0x4D     1        uint8       vibrato_depth    Auto-vibrato depth
0x4E     1        uint8       vibrato_rate     Auto-vibrato rate
0x4F     1        uint8       vibrato_wave     Waveform (0=sine, 1=ramp, 2=square, 3=rand)
```

### 3.3 Instrument Header (`'IMPI'`, 554 Bytes)
```
Offset   Length   Type        Field Name       Description
-------------------------------------------------------------------------------
0x000    4        char[4]     magic            "IMPI" (0x49504D49)
0x004    12       char[12]    dos_filename     DOS 8.3 filename
0x010    1        uint8       zero             0
0x011    1        uint8       nna              New Note Action (0=Cut, 1=Cont, 2=Off, 3=Fade)
0x012    1        uint8       dct              Duplicate Check Type (0=Off, 1=Note, 2=Smp, 3=Inst)
0x013    1        uint8       dca              Duplicate Check Action (0=Cut, 1=Off, 2=Fade)
0x014    2        uint16      fadeout          Volume fadeout rate (0..1024)
0x016    1        uint8       pitchpan_sep     Pitch-Pan separation
0x017    1        uint8       pitchpan_center  Pitch-Pan center note
0x018    1        uint8       global_vol       Instrument global volume (0..128)
0x019    1        uint8       default_pan      Default pan (0..64, bit 7: disabled)
0x01A    2        uint16      random_var       Random volume / pan variation
0x01C    2        uint16      tracker_version  Tracker version
0x01E    1        uint8       num_samples      Number of associated samples
0x01F    1        uint8       reserved         0
0x020    26       char[26]    inst_name        Instrument name (null-padded)
0x03A    2        uint16      initial_filter   Filter cutoff / resonance
0x03C    4        uint32      midi_settings    MIDI channel, program, bank
0x040    240      pair[120]   keyboard_map     120 note-sample assignment pairs [Note, Sample]
0x130    82       struct      vol_envelope     Volume Envelope (Flags, Nodes, 25 Points)
0x182    82       struct      pan_envelope     Panning Envelope (Flags, Nodes, 25 Points)
0x1D4    82       struct      pitch_envelope   Pitch / Filter Envelope (Flags, Nodes, 25 Points)
```

---

## 4. Pattern Codec & Effect Translation

### 4.1 Pattern Packing Scheme
Each row is packed sequentially from row `0` to `num_rows - 1` and terminated by `0x00`:
* `Channel Variable`: Bits 0..6 specify channel index (`1..64`), bit 7 indicates whether a new `Mask Variable` byte follows.
* `Mask Variable`:
  * Bit 0: Note byte follows (`0..119`, `253`=Cut, `254`=Off, `255`=Fade).
  * Bit 1: Instrument byte follows (`1..99`).
  * Bit 2: Volume column byte follows.
  * Bit 3: Effect command (`1..26`) + Effect parameter (`0x00..0xFF`) follow.
  * Bits 4..7: Reuse previous Note, Instrument, Volume, or Effect for this channel.

### 4.2 Effect Command Codes (`ItCommand`)
```cpp
enum class ItCommand : uint8_t {
    None = 0,
    SetSpeed = 1,          // A
    JumpToOrder = 2,       // B
    PatternBreak = 3,      // C
    VolumeSlide = 4,       // D
    PitchSlideDown = 5,    // E
    PitchSlideUp = 6,      // F
    TonePortamento = 7,    // G
    Vibrato = 8,           // H
    Tremor = 9,            // I
    Arpeggio = 10,         // J
    DualVibratoVol = 11,   // K
    DualPortamentoVol = 12,// L
    ChannelVolume = 13,    // M
    ChannelVolSlide = 14,  // N
    SampleOffset = 15,     // O
    PanningSlide = 16,     // P
    RetriggerNote = 17,    // Q
    Tremolo = 18,          // R
    Extended = 19,         // S
    SetTempo = 20,         // T
    FineVibrato = 21,      // U
    GlobalVolume = 22,     // V
    GlobalVolSlide = 23,   // W
    SetPanning = 24,       // X
    Panbrello = 25,        // Y
    MidiMacro = 26         // Z
};
```

---

## 5. IT 2.14 / 2.15 Sample Decompression

IT compressed samples are processed in blocks of up to 32,768 ($0x8000$) sample frames using an adaptive bit-width LSB-first bitstream:

```cpp
namespace tracker::it {

Status decompress_it_sample_8(io::InputStream& stream,
                              std::vector<int8_t>& out_data,
                              uint32_t length_frames,
                              bool is_it215,
                              bool is_delta);

Status decompress_it_sample_16(io::InputStream& stream,
                               std::vector<int16_t>& out_data,
                               uint32_t length_frames,
                               bool is_it215,
                               bool is_delta);

} // namespace tracker::it
```

* **8-bit compression:** Initial bit width is 9. Escape codes adjust bit width ($1 \dots 9$).
* **16-bit compression:** Initial bit width is 17. Escape codes adjust bit width ($1 \dots 17$).
* **Integration:**
  * IT 2.14: Single delta integration ($s_n = s_{n-1} + \Delta$).
  * IT 2.15: Double delta integration ($d_n = d_{n-1} + \Delta, \; s_n = s_{n-1} + d_n$).

---

## 6. `ItReader` & `ItWriter` Interface

```cpp
namespace tracker::it {

class ItReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

class ItWriter {
public:
    static Status save(const Song& song, io::OutputStream& stream);
    static Result<std::vector<uint8_t>> save_to_memory(const Song& song);
    static Status save_to_file(const Song& song, const std::string& path);
};

} // namespace tracker::it
```

---

## 7. Verification and Testing Strategy
1. **`test_it_compression`**: Validates 8-bit & 16-bit IT 2.14 and IT 2.15 sample decompression against pre-compressed reference audio streams.
2. **`test_it_pattern_codec`**: Validates 64-channel row mask packing/unpacking, previous value caching, note actions (Cut/Off/Fade), and effect command mapping.
3. **`test_it_reader`**: Validates `'IMPM'` header parsing, pointer offset tables, embedded song comments, `'IMPI'` instruments, `'IMPS'` samples, envelopes, and C5 speeds.
4. **`test_it_writer`**: Validates IT module export, pointer calculation, instrument serialization, and uncompressed 8/16-bit audio output.
5. **`test_it_roundtrip`**: Validates 100% full song round-trip fidelity in memory and filesystem streams.
6. **`test_cross_format_it`**: Validates bidirectional conversions across all 3 formats: $\text{IT} \longleftrightarrow \text{XM} \longleftrightarrow \text{MOD}$.
