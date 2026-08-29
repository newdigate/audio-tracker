#pragma once
#include <tracker/model.hpp>
#include <cstdint>

namespace tracker::mod {

uint8_t period_to_note(uint16_t period) noexcept;
uint16_t note_to_period(uint8_t note) noexcept;

void unpack_cell(const uint8_t raw[4], Cell& out_cell) noexcept;
void pack_cell(const Cell& cell, uint8_t out_raw[4]) noexcept;

} // namespace tracker::mod
