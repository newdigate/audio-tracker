#include "test_main.hpp"
#include <tracker/mod/mod_cell.hpp>
#include <tracker/mod/mod_types.hpp>

TEST_CASE(ModCell_PeriodToNote) {
    REQUIRE_EQ(tracker::mod::period_to_note(0), 0);
    REQUIRE_EQ(tracker::mod::period_to_note(856), 13); // C-1
    REQUIRE_EQ(tracker::mod::period_to_note(428), 25); // C-2
    REQUIRE_EQ(tracker::mod::period_to_note(214), 37); // C-3
    REQUIRE_EQ(tracker::mod::period_to_note(113), 48); // B-3

    // Closest match snapping
    REQUIRE_EQ(tracker::mod::period_to_note(850), 13); // close to 856
    REQUIRE_EQ(tracker::mod::period_to_note(810), 14); // close to 808
}

TEST_CASE(ModCell_NoteToPeriod) {
    REQUIRE_EQ(tracker::mod::note_to_period(0), 0);
    REQUIRE_EQ(tracker::mod::note_to_period(13), 856);
    REQUIRE_EQ(tracker::mod::note_to_period(25), 428);
    REQUIRE_EQ(tracker::mod::note_to_period(97), 0); // Key off has no period
}

TEST_CASE(ModCell_PackAndUnpack) {
    tracker::Cell cell;
    cell.note = 13; // C-1 (period 856 = 0x0358)
    cell.instrument = 18; // 0x12
    cell.effect_type = 0x0C; // Set volume
    cell.effect_param = 0x3F;

    uint8_t raw[4] = {0};
    tracker::mod::pack_cell(cell, raw);

    // Byte 0: Sample high nibble (0x10) | Period high nibble (0x03) = 0x13
    REQUIRE_EQ(raw[0], 0x13);
    // Byte 1: Period low 8 bits (0x58) = 0x58
    REQUIRE_EQ(raw[1], 0x58);
    // Byte 2: Sample low nibble (0x20) | Effect (0x0C) = 0x2C
    REQUIRE_EQ(raw[2], 0x2C);
    // Byte 3: Param = 0x3F
    REQUIRE_EQ(raw[3], 0x3F);

    tracker::Cell unpacked;
    tracker::mod::unpack_cell(raw, unpacked);
    REQUIRE_EQ(unpacked.note, 13);
    REQUIRE_EQ(unpacked.instrument, 18);
    REQUIRE_EQ(unpacked.effect_type, 0x0C);
    REQUIRE_EQ(unpacked.effect_param, 0x3F);
}
