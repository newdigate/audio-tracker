#include <tracker/s3m/s3m_types.hpp>

namespace tracker::s3m {

uint8_t note_to_s3m_byte(uint8_t note) noexcept {
    if (note == 0) {
        return S3M_NOTE_NONE; // 0xFF
    }
    if (note == 97 || note == 254) {
        return S3M_NOTE_KEY_OFF; // 0xFE
    }
    // Notes are 1-based (1 = C-0)
    uint8_t octave = static_cast<uint8_t>((note - 1) / 12);
    uint8_t semitone = static_cast<uint8_t>((note - 1) % 12);
    return static_cast<uint8_t>((octave << 4) | (semitone & 0x0F));
}

uint8_t s3m_byte_to_note(uint8_t byte) noexcept {
    if (byte == S3M_NOTE_NONE) { // 0xFF
        return 0;
    }
    if (byte == S3M_NOTE_KEY_OFF) { // 0xFE
        return 97;
    }
    uint8_t octave = (byte >> 4) & 0x0F;
    uint8_t semitone = byte & 0x0F;
    return static_cast<uint8_t>(octave * 12 + semitone + 1);
}

} // namespace tracker::s3m
