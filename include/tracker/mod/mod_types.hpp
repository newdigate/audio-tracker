#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::mod {

constexpr size_t MOD_TITLE_LEN = 20;
constexpr size_t MOD_SAMPLE_NAME_LEN = 22;
constexpr size_t MOD_NUM_SAMPLES = 31;
constexpr size_t MOD_SAMPLE_HEADER_LEN = 30;
constexpr size_t MOD_ORDER_TABLE_LEN = 128;
constexpr size_t MOD_HEADER_LEN = 1084;
constexpr size_t MOD_PATTERN_ROWS = 64;

// Standard Amiga PAL period table for 5 octaves (C-0 to B-5, 60 notes)
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

} // namespace tracker::mod
