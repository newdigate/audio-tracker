#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace tracker {

struct Cell {
    uint8_t note{0};         // 0: None, 1..96: Note, 97: Key-Off (0x61)
    uint8_t instrument{0};   // 0: None, 1..128: 1-indexed instrument ID
    uint8_t volume{0};       // 0: None, 0x10..0x50: Vol 0..64, 0x60..0xFF: Vol effects
    uint8_t effect_type{0};  // 0..35 (0x00..0x23)
    uint8_t effect_param{0}; // 0x00..0xFF

    bool is_empty() const noexcept {
        return note == 0 && instrument == 0 && volume == 0 &&
               effect_type == 0 && effect_param == 0;
    }
};

class Pattern {
public:
    uint16_t num_rows{64};
    uint16_t num_channels{4};
    std::vector<Cell> cells;

    Pattern() = default;
    Pattern(uint16_t rows, uint16_t channels);

    Cell& get_cell(uint16_t row, uint16_t channel);
    const Cell& get_cell(uint16_t row, uint16_t channel) const;

    bool is_all_empty() const noexcept;
};

enum class LoopType : uint8_t {
    None = 0,
    Forward = 1,
    PingPong = 2
};

enum class NewNoteAction : uint8_t {
    Cut = 0,
    Continue = 1,
    NoteOff = 2,
    NoteFade = 3
};

enum class DuplicateCheckType : uint8_t {
    Off = 0,
    Note = 1,
    Sample = 2,
    Instrument = 3
};

enum class DuplicateCheckAction : uint8_t {
    Cut = 0,
    NoteOff = 1,
    NoteFade = 2
};

struct KeyboardNode {
    uint8_t note{0};    // Note to play (0..119)
    uint8_t sample{0};  // Sample index (1..99, 0 = none)
};

inline constexpr std::array<KeyboardNode, 120> default_keyboard_map() noexcept {
    std::array<KeyboardNode, 120> map{};
    for (size_t i = 0; i < 120; ++i) {
        map[i].note = static_cast<uint8_t>(i);
        map[i].sample = 0;
    }
    return map;
}

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
    int8_t relative_note{0};     // Relative note number (-96..+95)
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
    std::vector<EnvelopePoint> points;
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
    std::array<KeyboardNode, 120> keyboard_map{default_keyboard_map()};

    std::array<uint8_t, 96> sample_map{}; // XM note-to-sample map
    Envelope volume_envelope;
    Envelope panning_envelope;
    Envelope pitch_envelope;     // Pitch / Filter envelope [IT]

    uint8_t vibrato_type{0};
    uint8_t vibrato_sweep{0};
    uint8_t vibrato_depth{0};
    uint8_t vibrato_rate{0};

    std::vector<Sample> samples;
};

struct Song {
    std::string name;
    std::string tracker_name{"FastTracker v2.00   "};
    std::string message;         // Embedded song message / comments [IT]
    uint16_t version{0x0104};
    uint16_t restart_position{0};
    uint16_t num_channels{4};
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

} // namespace tracker
