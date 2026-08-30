#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::s3m {

constexpr const char* S3M_SIGNATURE_SONG = "SCRM";
constexpr size_t S3M_SIGNATURE_SONG_LEN = 4;
constexpr const char* S3M_SIGNATURE_SAMPLE = "SCRS";
constexpr size_t S3M_SIGNATURE_SAMPLE_LEN = 4;

constexpr const char* S3M_MAGIC_SCRM = "SCRM";
constexpr const char* S3M_MAGIC_SCRS = "SCRS";

constexpr uint32_t S3M_MAGIC_SCRM_U32   = 0x4D524353; // "SCRM" Little-Endian
constexpr uint32_t S3M_MAGIC_SCRS_U32   = 0x53524353; // "SCRS" Little-Endian

// Header and structure byte sizes
constexpr uint32_t S3M_HEADER_SIZE        = 96;
constexpr uint32_t S3M_SAMPLE_HEADER_SIZE = 80;

// Structural limits
constexpr size_t S3M_MAX_CHANNELS        = 32;
constexpr size_t S3M_MAX_INSTRUMENTS     = 99;
constexpr size_t S3M_MAX_SAMPLES         = 99;
constexpr size_t S3M_MAX_ORDERS          = 256;
constexpr size_t S3M_MAX_PATTERNS        = 100;
constexpr size_t S3M_ROWS_PER_PATTERN    = 64;

// Sample format and flags
constexpr uint16_t S3M_FFI_SIGNED        = 1;
constexpr uint16_t S3M_FFI_UNSIGNED      = 2;

constexpr uint8_t S3M_SAMPLE_TYPE_PCM    = 1;
constexpr uint8_t S3M_SAMPLE_LOOP        = 0x01;
constexpr uint8_t S3M_SAMPLE_STEREO      = 0x02;
constexpr uint8_t S3M_SAMPLE_16BIT       = 0x04;

// Channel settings & Panning
constexpr uint8_t S3M_CHANNEL_DISABLED    = 0xFF;
constexpr uint8_t S3M_DEFAULT_PANNING_TAG = 0xFC;

// Pattern packing bitmasks
constexpr uint8_t S3M_PACK_CHANNEL_MASK  = 0x1F;
constexpr uint8_t S3M_PACK_NOTE_INST     = 0x20;
constexpr uint8_t S3M_PACK_VOLUME        = 0x40;
constexpr uint8_t S3M_PACK_EFFECT        = 0x80;
constexpr uint8_t S3M_ROW_END            = 0x00;

// Special note values
constexpr uint8_t S3M_NOTE_KEY_OFF       = 0xFE;
constexpr uint8_t S3M_NOTE_NONE          = 0xFF;

// Special order values
constexpr uint8_t S3M_ORDER_END          = 255;
constexpr uint8_t S3M_ORDER_SKIP         = 254;

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

// Parapointer utilities
inline constexpr uint32_t parapointer_to_offset(uint32_t parapointer) noexcept {
    return parapointer * 16u;
}

inline constexpr uint16_t offset_to_parapointer(uint32_t offset) noexcept {
    return static_cast<uint16_t>(offset / 16u);
}

inline constexpr uint32_t align_paragraph_offset(uint32_t offset) noexcept {
    return (offset + 15u) & ~15u;
}

// Note conversion functions
uint8_t note_to_s3m_byte(uint8_t note) noexcept;
uint8_t s3m_byte_to_note(uint8_t byte) noexcept;

} // namespace tracker::s3m
