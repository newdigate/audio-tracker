#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::it {

constexpr const char* IT_SIGNATURE_SONG = "IMPM";
constexpr size_t IT_SIGNATURE_SONG_LEN = 4;
constexpr const char* IT_SIGNATURE_INST = "IMPI";
constexpr size_t IT_SIGNATURE_INST_LEN = 4;
constexpr const char* IT_SIGNATURE_SAMPLE = "IMPS";
constexpr size_t IT_SIGNATURE_SAMPLE_LEN = 4;

constexpr uint32_t IT_MAGIC_SONG_U32   = 0x4D504D49; // "IMPM" Little-Endian
constexpr uint32_t IT_MAGIC_INST_U32   = 0x49504D49; // "IMPI" Little-Endian
constexpr uint32_t IT_MAGIC_SAMPLE_U32 = 0x53504D49; // "IMPS" Little-Endian

// Header and structure byte sizes
constexpr uint32_t IT_HEADER_SIZE        = 192;
constexpr uint32_t IT_SAMPLE_HEADER_SIZE = 80;
constexpr uint32_t IT_INST_HEADER_SIZE   = 554;

// Structural limits
constexpr size_t IT_MAX_CHANNELS        = 64;
constexpr size_t IT_MAX_INSTRUMENTS     = 99;
constexpr size_t IT_MAX_SAMPLES         = 99;
constexpr size_t IT_MAX_ORDERS          = 256;
constexpr size_t IT_MAX_PATTERNS        = 200;
constexpr size_t IT_MAX_KEYMAP_NODES    = 120;
constexpr size_t IT_MAX_ENVELOPE_NODES  = 25;

// Sample flags (offset 0x12)
constexpr uint8_t IT_SAMPLE_EXISTS           = 0x01;
constexpr uint8_t IT_SAMPLE_16BIT            = 0x02;
constexpr uint8_t IT_SAMPLE_STEREO           = 0x04;
constexpr uint8_t IT_SAMPLE_COMPRESSED       = 0x08;
constexpr uint8_t IT_SAMPLE_LOOP             = 0x10;
constexpr uint8_t IT_SAMPLE_SUSTAIN_LOOP     = 0x20;
constexpr uint8_t IT_SAMPLE_PINGPONG_LOOP    = 0x40;
constexpr uint8_t IT_SAMPLE_PINGPONG_SUSTAIN = 0x80;

// Sample convert flags (offset 0x2E)
constexpr uint8_t IT_CONVERT_SIGNED          = 0x01;
constexpr uint8_t IT_CONVERT_BIG_ENDIAN      = 0x02;
constexpr uint8_t IT_CONVERT_DELTA           = 0x04;
constexpr uint8_t IT_CONVERT_BYTE_DELTA      = 0x08;
constexpr uint8_t IT_CONVERT_16BIT_TO_8BIT   = 0x10;
constexpr uint8_t IT_CONVERT_STEREO_PROMPT   = 0x20;

// Song flags (offset 0x2C)
constexpr uint16_t IT_SONG_STEREO            = 0x0001;
constexpr uint16_t IT_SONG_VOL0_MIX          = 0x0002;
constexpr uint16_t IT_SONG_INSTRUMENTS       = 0x0004;
constexpr uint16_t IT_SONG_LINEAR_SLIDES     = 0x0008;
constexpr uint16_t IT_SONG_OLD_FX            = 0x0010;
constexpr uint16_t IT_SONG_LINK_GXX_MEM      = 0x0020;

// Song special flags (offset 0x2E)
constexpr uint16_t IT_SPECIAL_MESSAGE        = 0x0001;

// Compression block parameters
constexpr uint32_t IT_COMPRESSION_BLOCK_SIZE_8  = 0x8000; // 32768 samples
constexpr uint32_t IT_COMPRESSION_BLOCK_SIZE_16 = 0x4000; // 16384 samples (32768 bytes)

// Pattern channel packing bitmasks
constexpr uint8_t IT_MASK_NOTE            = 0x01;
constexpr uint8_t IT_MASK_INSTRUMENT      = 0x02;
constexpr uint8_t IT_MASK_VOLUME          = 0x04;
constexpr uint8_t IT_MASK_EFFECT          = 0x08;
constexpr uint8_t IT_MASK_SAME_NOTE       = 0x10;
constexpr uint8_t IT_MASK_SAME_INSTRUMENT = 0x20;
constexpr uint8_t IT_MASK_SAME_VOLUME     = 0x40;
constexpr uint8_t IT_MASK_SAME_EFFECT     = 0x80;

constexpr uint8_t IT_CHANNEL_HAS_MASK     = 0x80;
constexpr uint8_t IT_CHANNEL_NUM_MASK     = 0x7F;
constexpr uint8_t IT_ROW_END              = 0x00;

// Special note values
constexpr uint8_t IT_NOTE_CUT             = 253;
constexpr uint8_t IT_NOTE_OFF             = 254;
constexpr uint8_t IT_NOTE_FADE            = 255;
constexpr uint8_t IT_ORDER_END            = 255;
constexpr uint8_t IT_ORDER_SKIP           = 254;

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

} // namespace tracker::it
