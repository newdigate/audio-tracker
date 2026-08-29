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
