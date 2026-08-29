#include "test_main.hpp"
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/xm/xm_types.hpp>

TEST_CASE(XmPatternCodec_RoundTrip) {
    tracker::Pattern pat(4, 2);
    // Row 0, Ch 0: Note with inst & vol
    pat.get_cell(0, 0).note = 49;
    pat.get_cell(0, 0).instrument = 2;
    pat.get_cell(0, 0).volume = 0x40;

    // Row 1, Ch 1: Effect only
    pat.get_cell(1, 1).effect_type = 0x0A;
    pat.get_cell(1, 1).effect_param = 0x0F;

    auto packed = tracker::xm::pack_pattern(pat);
    REQUIRE(!packed.empty());

    tracker::Pattern unpacked(4, 2);
    auto status = tracker::xm::unpack_pattern(packed.data(), packed.size(), unpacked);
    REQUIRE(status.is_ok());

    REQUIRE_EQ(unpacked.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked.get_cell(0, 0).instrument, 2);
    REQUIRE_EQ(unpacked.get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_type, 0x0A);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_param, 0x0F);
}

TEST_CASE(XmPatternCodec_AllEmptyPattern) {
    tracker::Pattern pat(64, 4);
    auto packed = tracker::xm::pack_pattern(pat);
    REQUIRE(packed.empty()); // Standard FT2 optimization: empty pattern produces 0 bytes
}

TEST_CASE(XmPatternCodec_UncompressedCell) {
    // Uncompressed 5-byte cell format: note, inst, vol, effect_type, effect_param (when note < 0x80)
    std::vector<uint8_t> raw = {
        36, 1, 0x40, 0x0F, 0x06 // Row 0, Ch 0
    };

    tracker::Pattern unpacked(1, 1);
    auto status = tracker::xm::unpack_pattern(raw.data(), raw.size(), unpacked);
    REQUIRE(status.is_ok());

    const auto& cell = unpacked.get_cell(0, 0);
    REQUIRE_EQ(cell.note, 36);
    REQUIRE_EQ(cell.instrument, 1);
    REQUIRE_EQ(cell.volume, 0x40);
    REQUIRE_EQ(cell.effect_type, 0x0F);
    REQUIRE_EQ(cell.effect_param, 0x06);
}

TEST_CASE(XmPatternCodec_TruncatedData) {
    tracker::Pattern pat(1, 1);

    // Truncated uncompressed cell (< 5 bytes)
    std::vector<uint8_t> truncated_uncompressed = {12, 1, 0x40};
    auto st1 = tracker::xm::unpack_pattern(truncated_uncompressed.data(), truncated_uncompressed.size(), pat);
    REQUIRE(!st1.is_ok());
    REQUIRE_EQ(st1.code, tracker::ErrorCode::CorruptPatternData);

    // Truncated packed cell (mask says note + inst follow, but only mask provided)
    std::vector<uint8_t> truncated_packed = {tracker::xm::XM_PACK_FLAG | tracker::xm::XM_PACK_NOTE | tracker::xm::XM_PACK_INSTRUMENT};
    auto st2 = tracker::xm::unpack_pattern(truncated_packed.data(), truncated_packed.size(), pat);
    REQUIRE(!st2.is_ok());
    REQUIRE_EQ(st2.code, tracker::ErrorCode::CorruptPatternData);

    // Mask says note + inst follow, but only note provided
    std::vector<uint8_t> truncated_packed2 = {
        static_cast<uint8_t>(tracker::xm::XM_PACK_FLAG | tracker::xm::XM_PACK_NOTE | tracker::xm::XM_PACK_INSTRUMENT),
        48
    };
    auto st3 = tracker::xm::unpack_pattern(truncated_packed2.data(), truncated_packed2.size(), pat);
    REQUIRE(!st3.is_ok());
    REQUIRE_EQ(st3.code, tracker::ErrorCode::CorruptPatternData);
}

TEST_CASE(XmPatternCodec_FullCellRoundTrip) {
    tracker::Pattern pat(2, 2);
    // Row 0, Ch 0: All fields populated
    pat.get_cell(0, 0).note = 96;
    pat.get_cell(0, 0).instrument = 128;
    pat.get_cell(0, 0).volume = 0x50;
    pat.get_cell(0, 0).effect_type = 0x21;
    pat.get_cell(0, 0).effect_param = 0xFE;

    // Row 0, Ch 1: empty cell

    // Row 1, Ch 0: note + effect_param only
    pat.get_cell(1, 0).note = 1;
    pat.get_cell(1, 0).effect_param = 0x05;

    // Row 1, Ch 1: key off note (97) + vol
    pat.get_cell(1, 1).note = 97;
    pat.get_cell(1, 1).volume = 0x10;

    auto packed = tracker::xm::pack_pattern(pat);
    tracker::Pattern unpacked(2, 2);
    auto status = tracker::xm::unpack_pattern(packed.data(), packed.size(), unpacked);
    REQUIRE(status.is_ok());

    REQUIRE_EQ(unpacked.get_cell(0, 0).note, 96);
    REQUIRE_EQ(unpacked.get_cell(0, 0).instrument, 128);
    REQUIRE_EQ(unpacked.get_cell(0, 0).volume, 0x50);
    REQUIRE_EQ(unpacked.get_cell(0, 0).effect_type, 0x21);
    REQUIRE_EQ(unpacked.get_cell(0, 0).effect_param, 0xFE);

    REQUIRE(unpacked.get_cell(0, 1).is_empty());

    REQUIRE_EQ(unpacked.get_cell(1, 0).note, 1);
    REQUIRE_EQ(unpacked.get_cell(1, 0).instrument, 0);
    REQUIRE_EQ(unpacked.get_cell(1, 0).volume, 0);
    REQUIRE_EQ(unpacked.get_cell(1, 0).effect_type, 0);
    REQUIRE_EQ(unpacked.get_cell(1, 0).effect_param, 0x05);

    REQUIRE_EQ(unpacked.get_cell(1, 1).note, 97);
    REQUIRE_EQ(unpacked.get_cell(1, 1).instrument, 0);
    REQUIRE_EQ(unpacked.get_cell(1, 1).volume, 0x10);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_type, 0);
    REQUIRE_EQ(unpacked.get_cell(1, 1).effect_param, 0);
}
