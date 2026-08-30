#include "test_main.hpp"
#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <vector>

TEST_CASE(ItPatternCodec_RoundTrip) {
    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 64, Effect SetSpeed 6
    auto& c = pat.get_cell(0, 0);
    c.note = 49;
    c.instrument = 1;
    c.volume = 0x50; // Vol 64
    c.effect_type = 1; // SetSpeed
    c.effect_param = 6;

    // Row 1, Ch 0: Reuse note & inst, new volume
    auto& c2 = pat.get_cell(1, 0);
    c2.note = 49;
    c2.instrument = 1;
    c2.volume = 0x48; // Vol 56

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 64, 4);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.num_channels, 4);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).volume, 0x50);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_type, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_param, 6);

    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).volume, 0x48);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).effect_type, 0);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).effect_param, 0);
}

TEST_CASE(ItPatternCodec_AllEmptyPattern) {
    tracker::Pattern pat(32, 8);
    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());
    REQUIRE_EQ(out.data().size(), 32); // 32 rows of 0x00 (IT_ROW_END)

    for (size_t i = 0; i < 32; ++i) {
        REQUIRE_EQ(out.data()[i], tracker::it::IT_ROW_END);
    }

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 32, 8);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 32);
    REQUIRE_EQ(unpacked_pat.num_channels, 8);
    REQUIRE(unpacked_pat.is_all_empty());
}

TEST_CASE(ItPatternCodec_64Channels) {
    tracker::Pattern pat(16, 64);
    for (uint16_t ch = 0; ch < 64; ++ch) {
        auto& c = pat.get_cell(ch % 16, ch);
        c.note = static_cast<uint8_t>((ch % 120) + 1);
        c.instrument = static_cast<uint8_t>((ch % 99) + 1);
        c.volume = static_cast<uint8_t>(ch % 65);
        c.effect_type = static_cast<uint8_t>((ch % 26) + 1);
        c.effect_param = static_cast<uint8_t>(ch * 3);
    }

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 16, 64);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 16);
    REQUIRE_EQ(unpacked_pat.num_channels, 64);

    for (uint16_t ch = 0; ch < 64; ++ch) {
        const auto& c = unpacked_pat.get_cell(ch % 16, ch);
        REQUIRE_EQ(c.note, static_cast<uint8_t>((ch % 120) + 1));
        REQUIRE_EQ(c.instrument, static_cast<uint8_t>((ch % 99) + 1));
        REQUIRE_EQ(c.volume, static_cast<uint8_t>(ch % 65));
        REQUIRE_EQ(c.effect_type, static_cast<uint8_t>((ch % 26) + 1));
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(ch * 3));
    }
}

TEST_CASE(ItPatternCodec_ConsecutiveSameMaskAndValues) {
    tracker::Pattern pat(8, 2);
    // Across rows 0..4, ch 0 has identical note & effect, with different volumes
    for (uint16_t row = 0; row < 5; ++row) {
        auto& c = pat.get_cell(row, 0);
        c.note = 60;
        c.instrument = 2;
        c.volume = static_cast<uint8_t>(10 + row);
        c.effect_type = static_cast<uint8_t>(tracker::it::ItCommand::Vibrato);
        c.effect_param = 0x44;
    }

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 8, 2);
    REQUIRE(unpack_st.is_ok());

    for (uint16_t row = 0; row < 5; ++row) {
        const auto& c = unpacked_pat.get_cell(row, 0);
        REQUIRE_EQ(c.note, 60);
        REQUIRE_EQ(c.instrument, 2);
        REQUIRE_EQ(c.volume, static_cast<uint8_t>(10 + row));
        REQUIRE_EQ(c.effect_type, static_cast<uint8_t>(tracker::it::ItCommand::Vibrato));
        REQUIRE_EQ(c.effect_param, 0x44);
    }
}

TEST_CASE(ItPatternCodec_SpecialNotes) {
    tracker::Pattern pat(4, 1);
    pat.get_cell(0, 0).note = tracker::it::IT_NOTE_CUT;  // 253
    pat.get_cell(1, 0).note = tracker::it::IT_NOTE_OFF;  // 254
    pat.get_cell(2, 0).note = tracker::it::IT_NOTE_FADE; // 255
    pat.get_cell(3, 0).note = 1;                         // C-0

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 4, 1);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, tracker::it::IT_NOTE_CUT);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).note, tracker::it::IT_NOTE_OFF);
    REQUIRE_EQ(unpacked_pat.get_cell(2, 0).note, tracker::it::IT_NOTE_FADE);
    REQUIRE_EQ(unpacked_pat.get_cell(3, 0).note, 1);
}

TEST_CASE(ItPatternCodec_TruncatedData) {
    tracker::Pattern pat(4, 2);
    pat.get_cell(0, 0).note = 48;
    pat.get_cell(0, 0).instrument = 1;
    pat.get_cell(0, 0).volume = 64;
    pat.get_cell(0, 0).effect_type = 1;
    pat.get_cell(0, 0).effect_param = 6;

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    // Truncate the packed stream halfway through row 0
    std::vector<uint8_t> truncated(out.data().begin(), out.data().begin() + 3);
    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(truncated.data(), truncated.size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 4, 2);
    REQUIRE(!unpack_st.is_ok());
    REQUIRE(unpack_st.code == tracker::ErrorCode::UnexpectedEof || unpack_st.code == tracker::ErrorCode::CorruptPatternData);
}

TEST_CASE(ItPatternCodec_IgnoreOutOfRangeChannels) {
    // Manually create packed pattern with channel 5 in a 2-channel destination pattern
    std::vector<uint8_t> packed;
    // Row 0: Ch 5 (index 4) with mask = 0x01 (Note follows), note = 50
    packed.push_back(5 | tracker::it::IT_CHANNEL_HAS_MASK); // channel 5 + has mask
    packed.push_back(tracker::it::IT_MASK_NOTE); // mask = note
    packed.push_back(50); // note = 50
    // Ch 1 (index 0) with mask = 0x01, note = 40
    packed.push_back(1 | tracker::it::IT_CHANNEL_HAS_MASK);
    packed.push_back(tracker::it::IT_MASK_NOTE);
    packed.push_back(40);
    packed.push_back(tracker::it::IT_ROW_END); // end row 0

    // Row 1: empty
    packed.push_back(tracker::it::IT_ROW_END);

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(packed.data(), packed.size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked_pat, 2, 2);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 2);
    REQUIRE_EQ(unpacked_pat.num_channels, 2);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 40);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 1).note, 0); // channel 1 (0-indexed) was not touched
}

TEST_CASE(ItPatternCodec_All26Effects) {
    tracker::Pattern pat(26, 1);
    for (uint8_t i = 1; i <= 26; ++i) {
        auto& c = pat.get_cell(i - 1, 0);
        c.note = 48;
        c.instrument = 1;
        c.effect_type = i;
        c.effect_param = static_cast<uint8_t>(i * 7);
    }

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked, 26, 1);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked.num_rows, 26);

    for (uint8_t i = 1; i <= 26; ++i) {
        const auto& c = unpacked.get_cell(i - 1, 0);
        REQUIRE_EQ(c.note, 48);
        REQUIRE_EQ(c.instrument, 1);
        REQUIRE_EQ(c.effect_type, i);
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(i * 7));
    }
}

TEST_CASE(ItPatternCodec_EdgeCases_ZeroRowsAndZeroChannels) {
    tracker::Pattern empty_rows_pat(0, 4);
    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::it::pack_pattern(empty_rows_pat, out);
    REQUIRE(pack_st.is_ok());
    REQUIRE_EQ(out.data().size(), 0);

    tracker::Pattern unpacked;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked, 0, 4);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked.num_rows, 0);
    REQUIRE_EQ(unpacked.num_channels, 4);

    // Channel count 0 should return error
    auto unpack_st2 = tracker::it::unpack_pattern(in, unpacked, 10, 0);
    REQUIRE(!unpack_st2.is_ok());
    REQUIRE_EQ(unpack_st2.code, tracker::ErrorCode::InvalidChannelCount);
}

TEST_CASE(ItPatternCodec_InvalidChannelIndex) {
    // Channel number 0 with mask flag (0x80)
    std::vector<uint8_t> bad_data = {0x80, 0x01, 0x30, 0x00};
    tracker::Pattern unpacked;
    tracker::io::MemoryInputStream in(bad_data.data(), bad_data.size());
    auto unpack_st = tracker::it::unpack_pattern(in, unpacked, 1, 4);
    REQUIRE(!unpack_st.is_ok());
    REQUIRE_EQ(unpack_st.code, tracker::ErrorCode::CorruptPatternData);

    // Channel number > 64 (e.g. 65 = 0x41 | 0x80 = 0xC1)
    std::vector<uint8_t> bad_data2 = {0xC1, 0x01, 0x30, 0x00};
    tracker::io::MemoryInputStream in2(bad_data2.data(), bad_data2.size());
    auto unpack_st2 = tracker::it::unpack_pattern(in2, unpacked, 1, 4);
    REQUIRE(!unpack_st2.is_ok());
    REQUIRE_EQ(unpack_st2.code, tracker::ErrorCode::CorruptPatternData);
}

