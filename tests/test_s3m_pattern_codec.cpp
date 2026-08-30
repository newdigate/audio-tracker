#include "test_main.hpp"
#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <vector>

TEST_CASE(S3mPatternCodec_RoundTrip) {
    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 64, Effect SetSpeed 6
    auto& c = pat.get_cell(0, 0);
    c.note = 49;
    c.instrument = 1;
    c.volume = 0x50; // Vol 64
    c.effect_type = 1; // SetSpeed
    c.effect_param = 6;

    // Row 10, Ch 2: Note 25 (C-2), Inst 2, Vol 32
    auto& c2 = pat.get_cell(10, 2);
    c2.note = 25;
    c2.instrument = 2;
    c2.volume = 0x30; // Vol 32

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 4);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.num_channels, 4);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 49);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).volume, 0x50);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_type, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).effect_param, 6);

    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).note, 25);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).instrument, 2);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).volume, 0x30);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).effect_type, 0);
    REQUIRE_EQ(unpacked_pat.get_cell(10, 2).effect_param, 0);
}

TEST_CASE(S3mPatternCodec_AllEmptyPattern) {
    tracker::Pattern pat(64, 8);
    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());
    REQUIRE_EQ(out.data().size(), 64); // 64 rows of 0x00 (S3M_ROW_END)

    for (size_t i = 0; i < 64; ++i) {
        REQUIRE_EQ(out.data()[i], tracker::s3m::S3M_ROW_END);
    }

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 8);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.num_channels, 8);
    REQUIRE(unpacked_pat.is_all_empty());
}

TEST_CASE(S3mPatternCodec_32Channels) {
    tracker::Pattern pat(64, 32);
    for (uint16_t ch = 0; ch < 32; ++ch) {
        auto& c = pat.get_cell(ch * 2, ch);
        c.note = static_cast<uint8_t>((ch % 96) + 1);
        c.instrument = static_cast<uint8_t>((ch % 99) + 1);
        c.volume = static_cast<uint8_t>((ch % 64) + 1);
        c.effect_type = static_cast<uint8_t>((ch % 24) + 1);
        c.effect_param = static_cast<uint8_t>(ch * 5);
    }

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 32);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.num_channels, 32);

    for (uint16_t ch = 0; ch < 32; ++ch) {
        const auto& c = unpacked_pat.get_cell(ch * 2, ch);
        REQUIRE_EQ(c.note, static_cast<uint8_t>((ch % 96) + 1));
        REQUIRE_EQ(c.instrument, static_cast<uint8_t>((ch % 99) + 1));
        REQUIRE_EQ(c.volume, static_cast<uint8_t>((ch % 64) + 1));
        REQUIRE_EQ(c.effect_type, static_cast<uint8_t>((ch % 24) + 1));
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(ch * 5));
    }
}

TEST_CASE(S3mPatternCodec_SpecialNotes) {
    tracker::Pattern pat(64, 1);
    pat.get_cell(0, 0).note = 97; // Key Off (0xFE in S3M)
    pat.get_cell(1, 0).note = 1;  // C-0
    pat.get_cell(2, 0).note = 120;// B-9
    pat.get_cell(3, 0).note = 0;  // None
    pat.get_cell(3, 0).instrument = 5; // Inst only

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 1);
    REQUIRE(unpack_st.is_ok());

    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 97);
    REQUIRE_EQ(unpacked_pat.get_cell(1, 0).note, 1);
    REQUIRE_EQ(unpacked_pat.get_cell(2, 0).note, 120);
    REQUIRE_EQ(unpacked_pat.get_cell(3, 0).note, 0);
    REQUIRE_EQ(unpacked_pat.get_cell(3, 0).instrument, 5);
}

TEST_CASE(S3mPatternCodec_All26Effects) {
    tracker::Pattern pat(64, 1);
    for (uint8_t i = 1; i <= 26; ++i) {
        auto& c = pat.get_cell(i - 1, 0);
        c.note = 48;
        c.instrument = 1;
        c.effect_type = i;
        c.effect_param = static_cast<uint8_t>(i * 7);
    }

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    tracker::Pattern unpacked;
    tracker::io::MemoryInputStream in(out.data().data(), out.data().size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked, 1);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked.num_rows, 64);

    for (uint8_t i = 1; i <= 26; ++i) {
        const auto& c = unpacked.get_cell(i - 1, 0);
        REQUIRE_EQ(c.note, 48);
        REQUIRE_EQ(c.instrument, 1);
        REQUIRE_EQ(c.effect_type, i);
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(i * 7));
    }
}

TEST_CASE(S3mPatternCodec_TruncatedData) {
    tracker::Pattern pat(64, 2);
    pat.get_cell(0, 0).note = 48;
    pat.get_cell(0, 0).instrument = 1;
    pat.get_cell(0, 0).volume = 64;
    pat.get_cell(0, 0).effect_type = 1;
    pat.get_cell(0, 0).effect_param = 6;

    tracker::io::MemoryOutputStream out;
    auto pack_st = tracker::s3m::pack_pattern(pat, out);
    REQUIRE(pack_st.is_ok());

    // Truncate the packed stream halfway through row 0
    std::vector<uint8_t> truncated(out.data().begin(), out.data().begin() + 3);
    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(truncated.data(), truncated.size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 2);
    REQUIRE(!unpack_st.is_ok());
    REQUIRE((unpack_st.code == tracker::ErrorCode::UnexpectedEof || unpack_st.code == tracker::ErrorCode::CorruptPatternData));
}

TEST_CASE(S3mPatternCodec_IgnoreOutOfRangeChannels) {
    // Manually create packed pattern with channel 5 in a 2-channel destination pattern
    std::vector<uint8_t> packed;
    // Row 0: Ch 5 with S3M_PACK_NOTE_INST (0x20), note = 0x40 (Note 49), inst = 1
    packed.push_back(5 | tracker::s3m::S3M_PACK_NOTE_INST);
    packed.push_back(0x40); // note C-4
    packed.push_back(1);    // inst 1

    // Ch 0 with S3M_PACK_NOTE_INST (0x20), note = 0x30 (Note 37), inst = 2
    packed.push_back(0 | tracker::s3m::S3M_PACK_NOTE_INST);
    packed.push_back(0x30); // note C-3
    packed.push_back(2);    // inst 2

    packed.push_back(tracker::s3m::S3M_ROW_END); // end row 0

    // Remaining 63 rows empty
    for (int r = 1; r < 64; ++r) {
        packed.push_back(tracker::s3m::S3M_ROW_END);
    }

    tracker::Pattern unpacked_pat;
    tracker::io::MemoryInputStream in(packed.data(), packed.size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked_pat, 2);
    REQUIRE(unpack_st.is_ok());
    REQUIRE_EQ(unpacked_pat.num_rows, 64);
    REQUIRE_EQ(unpacked_pat.num_channels, 2);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).note, 37);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 0).instrument, 2);
    REQUIRE_EQ(unpacked_pat.get_cell(0, 1).note, 0); // channel 1 (0-indexed) was not touched
}

TEST_CASE(S3mPatternCodec_ZeroChannelsError) {
    tracker::Pattern unpacked;
    std::vector<uint8_t> empty_data(64, 0);
    tracker::io::MemoryInputStream in(empty_data.data(), empty_data.size());
    auto unpack_st = tracker::s3m::unpack_pattern(in, unpacked, 0);
    REQUIRE(!unpack_st.is_ok());
    REQUIRE_EQ(unpack_st.code, tracker::ErrorCode::InvalidChannelCount);
}
