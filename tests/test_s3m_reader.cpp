#include "test_main.hpp"
#include <tracker/s3m/s3m_reader.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

TEST_CASE(S3mReader_InvalidHeader) {
    std::vector<uint8_t> short_data(50, 0);
    auto res = tracker::s3m::S3mReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);

    // Null pointer
    auto res_null = tracker::s3m::S3mReader::load_from_memory(nullptr, 0);
    REQUIRE(!res_null.is_ok());
    REQUIRE_EQ(res_null.status().code, tracker::ErrorCode::InvalidSignature);

    // 96 bytes with wrong magic
    std::vector<uint8_t> wrong_magic(96, 0);
    wrong_magic[0x2C] = 'X'; wrong_magic[0x2D] = 'X'; wrong_magic[0x2E] = 'X'; wrong_magic[0x2F] = 'X';
    auto res_magic = tracker::s3m::S3mReader::load_from_memory(wrong_magic.data(), wrong_magic.size());
    REQUIRE(!res_magic.is_ok());
    REQUIRE_EQ(res_magic.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(S3mReader_MinimalSong) {
    tracker::io::MemoryOutputStream out;

    // Song title: 28 bytes
    std::string name = "Minimal S3M Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A); // dos_eof
    out.write_u8(0x10); // file_type = ST3 module
    out.write_u16_le(0); // reserved1
    out.write_u16_le(2); // ordnum = 2
    out.write_u16_le(0); // insnum = 0
    out.write_u16_le(0); // patnum = 0
    out.write_u16_le(0); // flags
    out.write_u16_le(0x1320); // cwt_vwt = ST3.20
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED); // ffi = 2 (unsigned)
    out.write("SCRM", 4); // magic
    out.write_u8(64);  // global_vol = 64
    out.write_u8(6);   // initial_speed = 6
    out.write_u8(125); // initial_tempo = 125
    out.write_u8(0x80 | 48); // master_volume (stereo | 48)
    out.write_u8(0);   // ultraclick
    out.write_u8(0);   // default_pan_tag (no custom pan table)
    for (size_t i = 0; i < 8; ++i) out.write_u8(0); // reserved2
    out.write_u16_le(0); // special_ptr

    // Channel settings: 32 bytes (first 4 enabled: 0, 1, 2, 3; remaining disabled 0xFF)
    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    // Orders (2 bytes)
    out.write_u8(0);
    out.write_u8(tracker::s3m::S3M_ORDER_END); // 255

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.name, "Minimal S3M Song");
    REQUIRE_EQ(song.tracker_name, "Scream Tracker");
    REQUIRE_EQ(song.version, 0x1320);
    REQUIRE(!song.linear_frequency);
    REQUIRE_EQ(song.global_volume, 64);
    REQUIRE_EQ(song.default_speed, 6);
    REQUIRE_EQ(song.default_bpm, 125);
    REQUIRE_EQ(song.num_channels, 4);
    REQUIRE_EQ(song.order_table.size(), 2);
    REQUIRE_EQ(song.order_table[0], 0);
    REQUIRE_EQ(song.order_table[1], 255);
}

TEST_CASE(S3mReader_SampleMode_8BitUnsigned) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Sample S3M Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1); // ordnum = 1
    out.write_u16_le(1); // insnum = 1
    out.write_u16_le(0); // patnum = 0
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED); // unsigned PCM
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    // Order 0
    out.write_u8(0);

    // Sample parapointer array (1 entry = 2 bytes)
    // Offset after header (96) + orders (1) + smp_pp (2) = 99 bytes -> align to paragraph (112 = 0x70, pp = 7)
    uint32_t current_pos = static_cast<uint32_t>(out.data().size() + 2);
    uint32_t smp_hdr_offset = tracker::s3m::align_paragraph_offset(current_pos);
    uint16_t smp_pp = tracker::s3m::offset_to_parapointer(smp_hdr_offset);
    out.write_u16_le(smp_pp);

    // Pad to smp_hdr_offset
    while (out.data().size() < smp_hdr_offset) {
        out.write_u8(0);
    }

    // Sample PCM data will be placed after sample header (80 bytes) aligned to 16 bytes
    uint32_t pcm_offset = tracker::s3m::align_paragraph_offset(smp_hdr_offset + tracker::s3m::S3M_SAMPLE_HEADER_SIZE);
    uint32_t pcm_pp = tracker::s3m::offset_to_parapointer(pcm_offset);
    uint8_t memseg_hi = static_cast<uint8_t>((pcm_pp >> 16) & 0xFF);
    uint16_t memseg_lo = static_cast<uint16_t>(pcm_pp & 0xFFFF);

    // Sample Header (80 bytes)
    out.write_u8(tracker::s3m::S3M_SAMPLE_TYPE_PCM); // sample_type = 1
    std::string dos_name = "SAMPLE1 .SMP";
    out.write(dos_name.data(), dos_name.size());
    out.write_u8(memseg_hi);
    out.write_u16_le(memseg_lo);
    out.write_u32_le(8); // length = 8 bytes
    out.write_u32_le(2); // loop_start = 2
    out.write_u32_le(6); // loop_end = 6 -> loop_length = 4
    out.write_u8(45);    // volume = 45
    out.write_u8(0);     // dsk
    out.write_u8(0);     // pack
    out.write_u8(tracker::s3m::S3M_SAMPLE_LOOP); // flags = loop on
    out.write_u32_le(16726); // c5_speed = 16726 Hz
    for (size_t i = 0; i < 12; ++i) out.write_u8(0); // reserved
    std::string sname = "Bass Wave";
    out.write(sname.data(), sname.size());
    for (size_t i = sname.size(); i < 28; ++i) out.write_u8(0);
    out.write("SCRS", 4); // magic

    // Pad to pcm_offset
    while (out.data().size() < pcm_offset) {
        out.write_u8(0);
    }

    // PCM Audio data (8 bytes unsigned PCM)
    // Signed target: [0, 30, 60, 30, 0, -30, -60, -30]
    // Unsigned bytes = signed ^ 0x80: [128, 158, 188, 158, 128, 98, 68, 98]
    std::vector<int8_t> expected_signed = {0, 30, 60, 30, 0, -30, -60, -30};
    for (int8_t s : expected_signed) {
        out.write_u8(static_cast<uint8_t>(s ^ 0x80));
    }

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    const auto& inst = song.instruments[0];
    REQUIRE_EQ(inst.name, "Bass Wave");
    REQUIRE_EQ(inst.filename, "SAMPLE1 .SMP");
    REQUIRE_EQ(inst.samples.size(), 1);

    const auto& smp = inst.samples[0];
    REQUIRE_EQ(smp.name, "Bass Wave");
    REQUIRE_EQ(smp.volume, 45);
    REQUIRE_EQ(smp.c5_speed, 16726);
    REQUIRE_EQ(smp.length, 8);
    REQUIRE(smp.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(smp.loop_start, 2);
    REQUIRE_EQ(smp.loop_length, 4);
    REQUIRE(!smp.is_16bit);
    REQUIRE_EQ(smp.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(smp.data8[i], expected_signed[i]);
    }
}

TEST_CASE(S3mReader_SampleMode_8BitSigned) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Signed Sample Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_SIGNED); // signed PCM
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0); // Order 0

    uint32_t smp_hdr_offset = tracker::s3m::align_paragraph_offset(static_cast<uint32_t>(out.data().size() + 2));
    out.write_u16_le(tracker::s3m::offset_to_parapointer(smp_hdr_offset));

    while (out.data().size() < smp_hdr_offset) out.write_u8(0);

    uint32_t pcm_offset = tracker::s3m::align_paragraph_offset(smp_hdr_offset + tracker::s3m::S3M_SAMPLE_HEADER_SIZE);
    uint32_t pcm_pp = tracker::s3m::offset_to_parapointer(pcm_offset);

    out.write_u8(tracker::s3m::S3M_SAMPLE_TYPE_PCM);
    out.write("SMP_SIGN.SMP", 12);
    out.write_u8(static_cast<uint8_t>((pcm_pp >> 16) & 0xFF));
    out.write_u16_le(static_cast<uint16_t>(pcm_pp & 0xFFFF));
    out.write_u32_le(4); // length = 4
    out.write_u32_le(0);
    out.write_u32_le(0);
    out.write_u8(64);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0); // loop off
    out.write_u32_le(8363);
    for (size_t i = 0; i < 12; ++i) out.write_u8(0);
    std::string sname = "Signed Lead";
    out.write(sname.data(), sname.size());
    for (size_t i = sname.size(); i < 28; ++i) out.write_u8(0);
    out.write("SCRS", 4);

    while (out.data().size() < pcm_offset) out.write_u8(0);

    std::vector<int8_t> pcm = {-50, -20, 20, 50};
    for (int8_t b : pcm) out.write_i8(b);

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& smp = res.value().instruments[0].samples[0];
    REQUIRE_EQ(smp.length, 4);
    REQUIRE(smp.loop_type == tracker::LoopType::None);
    REQUIRE_EQ(smp.data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(smp.data8[i], pcm[i]);
    }
}

TEST_CASE(S3mReader_SampleMode_16Bit) {
    tracker::io::MemoryOutputStream out;

    std::string name = "16-Bit Sample Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_SIGNED); // signed 16-bit
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0); // Order 0

    uint32_t smp_hdr_offset = tracker::s3m::align_paragraph_offset(static_cast<uint32_t>(out.data().size() + 2));
    out.write_u16_le(tracker::s3m::offset_to_parapointer(smp_hdr_offset));

    while (out.data().size() < smp_hdr_offset) out.write_u8(0);

    uint32_t pcm_offset = tracker::s3m::align_paragraph_offset(smp_hdr_offset + tracker::s3m::S3M_SAMPLE_HEADER_SIZE);
    uint32_t pcm_pp = tracker::s3m::offset_to_parapointer(pcm_offset);

    out.write_u8(tracker::s3m::S3M_SAMPLE_TYPE_PCM);
    out.write("SMP16BIT.SMP", 12);
    out.write_u8(static_cast<uint8_t>((pcm_pp >> 16) & 0xFF));
    out.write_u16_le(static_cast<uint16_t>(pcm_pp & 0xFFFF));
    out.write_u32_le(4); // length = 4 frames
    out.write_u32_le(0);
    out.write_u32_le(0);
    out.write_u8(64);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(tracker::s3m::S3M_SAMPLE_16BIT); // flags = 16-bit
    out.write_u32_le(22050); // c5_speed
    for (size_t i = 0; i < 12; ++i) out.write_u8(0);
    std::string sname = "16Bit Synth";
    out.write(sname.data(), sname.size());
    for (size_t i = sname.size(); i < 28; ++i) out.write_u8(0);
    out.write("SCRS", 4);

    while (out.data().size() < pcm_offset) out.write_u8(0);

    std::vector<int16_t> pcm16 = {-15000, -5000, 5000, 15000};
    for (int16_t v : pcm16) out.write_i16_le(v);

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& smp = res.value().instruments[0].samples[0];
    REQUIRE(smp.is_16bit);
    REQUIRE_EQ(smp.length, 4);
    REQUIRE_EQ(smp.c5_speed, 22050);
    REQUIRE_EQ(smp.data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(smp.data16[i], pcm16[i]);
    }
}

TEST_CASE(S3mReader_PatternData) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Pattern S3M Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1); // ordnum = 1
    out.write_u16_le(0); // insnum = 0
    out.write_u16_le(1); // patnum = 1
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED);
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0); // Order 0 points to Pattern 0

    // Pattern parapointer (1 entry = 2 bytes)
    uint32_t current_pos = static_cast<uint32_t>(out.data().size() + 2);
    uint32_t pat_offset = tracker::s3m::align_paragraph_offset(current_pos);
    out.write_u16_le(tracker::s3m::offset_to_parapointer(pat_offset));

    while (out.data().size() < pat_offset) out.write_u8(0);

    // Construct test pattern
    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 0x40 (Vol 48), Effect SetSpeed 6
    auto& c0 = pat.get_cell(0, 0);
    c0.note = 49;
    c0.instrument = 1;
    c0.volume = 48;
    c0.effect_type = static_cast<uint8_t>(tracker::s3m::S3mCommand::SetSpeed);
    c0.effect_param = 6;

    // Row 5, Ch 2: Note Key-Off (97), Inst 2
    auto& c1 = pat.get_cell(5, 2);
    c1.note = 97;
    c1.instrument = 2;

    tracker::io::MemoryOutputStream packed_pat;
    REQUIRE(tracker::s3m::pack_pattern(pat, packed_pat).is_ok());

    uint16_t packed_len = static_cast<uint16_t>(packed_pat.data().size());
    out.write_u16_le(packed_len);
    out.write(packed_pat.data().data(), packed_pat.data().size());

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.patterns.size(), 1);
    const auto& read_pat = song.patterns[0];
    REQUIRE_EQ(read_pat.num_rows, 64);
    REQUIRE_EQ(read_pat.num_channels, 4);

    REQUIRE_EQ(read_pat.get_cell(0, 0).note, 49);
    REQUIRE_EQ(read_pat.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(read_pat.get_cell(0, 0).volume, 48);
    REQUIRE_EQ(read_pat.get_cell(0, 0).effect_type, static_cast<uint8_t>(tracker::s3m::S3mCommand::SetSpeed));
    REQUIRE_EQ(read_pat.get_cell(0, 0).effect_param, 6);

    REQUIRE_EQ(read_pat.get_cell(5, 2).note, 97);
    REQUIRE_EQ(read_pat.get_cell(5, 2).instrument, 2);
}

TEST_CASE(S3mReader_CustomPanning) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Custom Pan S3M";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED);
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(tracker::s3m::S3M_DEFAULT_PANNING_TAG); // 0xFC (custom panning table present)
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0); // Order 0

    // Custom panning table: 32 bytes
    // Ch 0: Far Left (0x20 | 0x00)
    // Ch 1: Center (0x20 | 0x08)
    // Ch 2: Far Right (0x20 | 0x0F)
    // Ch 3: Default unset (0x00)
    out.write_u8(0x20 | 0x00);
    out.write_u8(0x20 | 0x08);
    out.write_u8(0x20 | 0x0F);
    out.write_u8(0x00);
    for (size_t c = 4; c < 32; ++c) out.write_u8(0x00);

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.channel_panning[0], 0);  // Far left (0 * 4 = 0)
    REQUIRE_EQ(song.channel_panning[1], 32); // Center (8 * 4 = 32)
    REQUIRE_EQ(song.channel_panning[2], 60); // Far right (15 * 4 = 60)
}

TEST_CASE(S3mReader_FileStream) {
    tracker::io::MemoryOutputStream out;

    std::string name = "File S3M Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED);
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0);

    const std::string tmp_path = "tmp_test_reader.s3m";
    {
        auto file_out = tracker::io::FileOutputStream::open(tmp_path);
        REQUIRE(file_out.is_ok());
        file_out.value().write(out.data().data(), out.data().size());
    }

    auto res = tracker::s3m::S3mReader::load_from_file(tmp_path);
    REQUIRE(res.is_ok());
    REQUIRE_EQ(res.value().name, "File S3M Song");

    std::remove(tmp_path.c_str());
}

TEST_CASE(S3mReader_FileNotFound) {
    auto res = tracker::s3m::S3mReader::load_from_file("non_existent_file_path.s3m");
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::IoError);
}

TEST_CASE(S3mReader_NullParapointers) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Null Pointers Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1); // 1 order
    out.write_u16_le(2); // 2 samples
    out.write_u16_le(2); // 2 patterns
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED);
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0); // Order 0

    // Sample parapointers (both 0 = empty)
    out.write_u16_le(0);
    out.write_u16_le(0);

    // Pattern parapointers (both 0 = empty)
    out.write_u16_le(0);
    out.write_u16_le(0);

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 2);
    REQUIRE_EQ(song.instruments[0].samples.size(), 0);
    REQUIRE_EQ(song.instruments[1].samples.size(), 0);
    REQUIRE_EQ(song.patterns.size(), 2);
    REQUIRE_EQ(song.patterns[0].num_rows, 64);
    REQUIRE_EQ(song.patterns[1].num_rows, 64);
    REQUIRE(song.patterns[0].is_all_empty());
    REQUIRE(song.patterns[1].is_all_empty());
}

TEST_CASE(S3mReader_InvalidSampleSignature) {
    tracker::io::MemoryOutputStream out;

    std::string name = "Corrupt Sample Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 28; ++i) out.write_u8(0);

    out.write_u8(0x1A);
    out.write_u8(0x10);
    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(1); // 1 sample
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x1320);
    out.write_u16_le(tracker::s3m::S3M_FFI_UNSIGNED);
    out.write("SCRM", 4);
    out.write_u8(64);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(48);
    out.write_u8(0);
    out.write_u8(0);
    for (size_t i = 0; i < 8; ++i) out.write_u8(0);
    out.write_u16_le(0);

    for (size_t c = 0; c < 4; ++c) out.write_u8(static_cast<uint8_t>(c));
    for (size_t c = 4; c < 32; ++c) out.write_u8(tracker::s3m::S3M_CHANNEL_DISABLED);

    out.write_u8(0);

    uint32_t smp_hdr_offset = tracker::s3m::align_paragraph_offset(static_cast<uint32_t>(out.data().size() + 2));
    out.write_u16_le(tracker::s3m::offset_to_parapointer(smp_hdr_offset));

    while (out.data().size() < smp_hdr_offset) out.write_u8(0);

    // Corrupt sample header with magic "XXXX" instead of "SCRS"
    out.write_u8(tracker::s3m::S3M_SAMPLE_TYPE_PCM);
    out.write("BADSMP  .SMP", 12);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(4);
    out.write_u32_le(0);
    out.write_u32_le(0);
    out.write_u8(64);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u32_le(8363);
    for (size_t i = 0; i < 12; ++i) out.write_u8(0);
    out.write("Bad Sample                  ", 28);
    out.write("XXXX", 4); // Invalid magic

    auto res = tracker::s3m::S3mReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    // Because magic wasn't SCRS, sample wasn't added to instrument
    REQUIRE_EQ(song.instruments[0].samples.size(), 0);
}

