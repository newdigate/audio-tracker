#include "test_main.hpp"
#include <tracker/xm/xm_reader.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

// Helper to construct a synthetic valid XM file in memory
struct SyntheticXmBuilder {
    tracker::io::MemoryOutputStream out;

    void write_header(const std::string& name, const std::string& tracker,
                      uint16_t version, uint16_t song_len, uint16_t restart_pos,
                      uint16_t num_channels, uint16_t num_patterns, uint16_t num_instruments,
                      uint16_t flags, uint16_t speed, uint16_t bpm,
                      const std::vector<uint8_t>& orders) {
        // 17 bytes signature
        out.write_fixed_string(tracker::xm::XM_SIGNATURE, 17);
        // 20 bytes song name
        out.write_fixed_string(name, 20);
        // 1 byte EOF
        out.write_u8(tracker::xm::XM_EOF_BYTE);
        // 20 bytes tracker name
        out.write_fixed_string(tracker, 20);
        // Version
        out.write_u16_le(version);
        // Header size (276 for version 0x0104)
        out.write_u32_le(tracker::xm::XM_HEADER_SIZE_104);
        // Song length
        out.write_u16_le(song_len);
        // Restart position
        out.write_u16_le(restart_pos);
        // Num channels
        out.write_u16_le(num_channels);
        // Num patterns
        out.write_u16_le(num_patterns);
        // Num instruments
        out.write_u16_le(num_instruments);
        // Flags
        out.write_u16_le(flags);
        // Default speed
        out.write_u16_le(speed);
        // Default BPM
        out.write_u16_le(bpm);
        // Order table (256 bytes)
        for (size_t i = 0; i < 256; ++i) {
            if (i < orders.size()) {
                out.write_u8(orders[i]);
            } else {
                out.write_u8(0);
            }
        }
    }

    void write_pattern(uint16_t num_rows, const std::vector<uint8_t>& packed_data) {
        // Pattern header length = 9
        out.write_u32_le(tracker::xm::XM_PATTERN_HEADER_LEN);
        // Packing type = 0
        out.write_u8(0);
        // Num rows
        out.write_u16_le(num_rows);
        // Packed data size
        out.write_u16_le(static_cast<uint16_t>(packed_data.size()));
        // Packed data
        if (!packed_data.empty()) {
            out.write(packed_data.data(), packed_data.size());
        }
    }

    const std::vector<uint8_t>& data() const {
        return out.data();
    }
};

TEST_CASE(XmReader_CorruptHeader) {
    std::vector<uint8_t> bad_data = {0, 1, 2, 3, 4};
    auto res = tracker::xm::XmReader::load_from_memory(bad_data.data(), bad_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);

    // Null pointer
    auto res_null = tracker::xm::XmReader::load_from_memory(nullptr, 0);
    REQUIRE(!res_null.is_ok());
    REQUIRE_EQ(res_null.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(XmReader_InvalidSignature) {
    std::vector<uint8_t> buffer(80, 0);
    // Write wrong signature
    std::memcpy(buffer.data(), "Extended Module:X", 17);
    auto res = tracker::xm::XmReader::load_from_memory(buffer.data(), buffer.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(XmReader_InvalidEofByte) {
    SyntheticXmBuilder builder;
    builder.write_header("Test Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 0, 0, 1, 6, 125, {0});
    auto data = builder.data();
    // Overwrite EOF byte (offset 37) with invalid value
    data[37] = 0x00;

    auto res = tracker::xm::XmReader::load_from_memory(data.data(), data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::CorruptHeader);
}

TEST_CASE(XmReader_FileNotFound) {
    auto res = tracker::xm::XmReader::load_from_file("non_existent_file_path_12345.xm");
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::IoError);
}

TEST_CASE(XmReader_MinimalValidModule) {
    SyntheticXmBuilder builder;
    builder.write_header("Simple Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         2, 0, 4, 1, 0, 1, 6, 125, {0, 0});

    // Pattern 0: 64 rows, empty packed data (size 0)
    builder.write_pattern(64, {});

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.name, "Simple Song");
    REQUIRE_EQ(song.tracker_name, "FastTracker v2.00");
    REQUIRE_EQ(song.version, 0x0104);
    REQUIRE_EQ(song.restart_position, 0);
    REQUIRE_EQ(song.num_channels, 4);
    REQUIRE(song.linear_frequency);
    REQUIRE_EQ(song.default_speed, 6);
    REQUIRE_EQ(song.default_bpm, 125);
    REQUIRE_EQ(song.order_table.size(), 2);
    REQUIRE_EQ(song.order_table[0], 0);
    REQUIRE_EQ(song.order_table[1], 0);
    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.patterns[0].num_rows, 64);
    REQUIRE_EQ(song.patterns[0].num_channels, 4);
    REQUIRE(song.patterns[0].is_all_empty());
    REQUIRE_EQ(song.instruments.size(), 0);
}

TEST_CASE(XmReader_PatternWithPackedData) {
    SyntheticXmBuilder builder;
    builder.write_header("Pattern Test", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 2, 1, 0, 1, 6, 125, {0});

    tracker::Pattern pat(4, 2);
    pat.get_cell(0, 0).note = 49;
    pat.get_cell(0, 0).instrument = 1;
    pat.get_cell(0, 0).volume = 0x40;
    pat.get_cell(1, 1).effect_type = 0x0A;
    pat.get_cell(1, 1).effect_param = 0x0F;

    auto packed = tracker::xm::pack_pattern(pat);
    builder.write_pattern(4, packed);

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.patterns[0].num_rows, 4);
    REQUIRE_EQ(song.patterns[0].num_channels, 2);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).note, 49);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(song.patterns[0].get_cell(1, 1).effect_type, 0x0A);
    REQUIRE_EQ(song.patterns[0].get_cell(1, 1).effect_param, 0x0F);
}

TEST_CASE(XmReader_InstrumentEmpty) {
    SyntheticXmBuilder builder;
    builder.write_header("Inst Empty", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 1, 1, 1, 6, 125, {0});
    builder.write_pattern(64, {});

    // Instrument with 0 samples (size 29)
    builder.out.write_u32_le(tracker::xm::XM_INST_HEADER_EMPTY_LEN);
    builder.out.write_fixed_string("Empty Inst", 22);
    builder.out.write_u8(0); // type
    builder.out.write_u16_le(0); // num_samples = 0

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    REQUIRE_EQ(song.instruments[0].name, "Empty Inst");
    REQUIRE_EQ(song.instruments[0].samples.size(), 0);
}

TEST_CASE(XmReader_InstrumentWith8BitSample) {
    SyntheticXmBuilder builder;
    builder.write_header("8bit Sample Test", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 1, 1, 1, 6, 125, {0});
    builder.write_pattern(64, {});

    // Full instrument header (size 263)
    builder.out.write_u32_le(tracker::xm::XM_INST_HEADER_FULL_LEN);
    builder.out.write_fixed_string("Lead Synth", 22);
    builder.out.write_u8(0); // type
    builder.out.write_u16_le(1); // num_samples = 1

    // Sample header size
    builder.out.write_u32_le(tracker::xm::XM_SAMPLE_HEADER_LEN);
    // 96 bytes sample map
    for (size_t i = 0; i < 96; ++i) builder.out.write_u8(0);

    // 12 volume envelope points
    for (size_t i = 0; i < 12; ++i) {
        builder.out.write_u16_le(i * 10);
        builder.out.write_u16_le(64 - i * 5);
    }
    // 12 panning envelope points
    for (size_t i = 0; i < 12; ++i) {
        builder.out.write_u16_le(i * 5);
        builder.out.write_u16_le(32);
    }
    builder.out.write_u8(4); // 4 volume points
    builder.out.write_u8(2); // 2 pan points
    builder.out.write_u8(2); // vol sustain
    builder.out.write_u8(0); // vol loop start
    builder.out.write_u8(3); // vol loop end
    builder.out.write_u8(1); // pan sustain
    builder.out.write_u8(0); // pan loop start
    builder.out.write_u8(1); // pan loop end
    builder.out.write_u8(7); // vol type: enabled (1) | sustain (2) | loop (4)
    builder.out.write_u8(1); // pan type: enabled (1)
    builder.out.write_u8(1); // vibrato type
    builder.out.write_u8(10); // vibrato sweep
    builder.out.write_u8(5);  // vibrato depth
    builder.out.write_u8(20); // vibrato rate
    builder.out.write_u16_le(256); // volume fadeout
    builder.out.write_zeros(22); // reserved (22 bytes to complete 263-byte header)

    // Sample 0 Header (40 bytes)
    std::vector<int8_t> pcm8 = {10, 20, 30, 40, 50, 40, 30, 20};
    auto delta8 = tracker::xm::encode_delta_8(pcm8);

    builder.out.write_u32_le(static_cast<uint32_t>(delta8.size())); // length
    builder.out.write_u32_le(2); // loop start
    builder.out.write_u32_le(4); // loop length
    builder.out.write_u8(64);    // volume
    builder.out.write_i8(5);     // finetune
    builder.out.write_u8(tracker::xm::XM_SAMPLE_LOOP_FORWARD); // flags: forward loop
    builder.out.write_u8(128);   // panning
    builder.out.write_i8(12);    // relative note
    builder.out.write_u8(0);     // reserved
    builder.out.write_fixed_string("Square Wave", 22);

    // Sample data
    builder.out.write(delta8.data(), delta8.size());

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    const auto& inst = song.instruments[0];
    REQUIRE_EQ(inst.name, "Lead Synth");
    REQUIRE_EQ(inst.vibrato_type, 1);
    REQUIRE_EQ(inst.vibrato_sweep, 10);
    REQUIRE_EQ(inst.vibrato_depth, 5);
    REQUIRE_EQ(inst.vibrato_rate, 20);
    REQUIRE_EQ(inst.volume_fadeout, 256);

    // Envelope checks
    REQUIRE(inst.volume_envelope.enabled);
    REQUIRE(inst.volume_envelope.sustain_enabled);
    REQUIRE(inst.volume_envelope.loop_enabled);
    REQUIRE_EQ(inst.volume_envelope.points.size(), 4);
    REQUIRE_EQ(inst.volume_envelope.points[0].tick, 0);
    REQUIRE_EQ(inst.volume_envelope.points[0].value, 64);
    REQUIRE_EQ(inst.volume_envelope.points[1].tick, 10);
    REQUIRE_EQ(inst.volume_envelope.points[1].value, 59);

    REQUIRE(inst.panning_envelope.enabled);
    REQUIRE(!inst.panning_envelope.sustain_enabled);
    REQUIRE_EQ(inst.panning_envelope.points.size(), 2);

    // Sample checks
    REQUIRE_EQ(inst.samples.size(), 1);
    const auto& s = inst.samples[0];
    REQUIRE_EQ(s.name, "Square Wave");
    REQUIRE_EQ(s.length, 8);
    REQUIRE_EQ(s.loop_start, 2);
    REQUIRE_EQ(s.loop_length, 4);
    REQUIRE_EQ(s.volume, 64);
    REQUIRE_EQ(s.finetune, 5);
    REQUIRE(s.loop_type == tracker::LoopType::Forward);
    REQUIRE(!s.is_16bit);
    REQUIRE_EQ(s.panning, 128);
    REQUIRE_EQ(s.relative_note, 12);
    REQUIRE_EQ(s.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(s.data8[i], pcm8[i]);
    }
}

TEST_CASE(XmReader_InstrumentWith16BitSample) {
    SyntheticXmBuilder builder;
    builder.write_header("16bit Sample Test", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 1, 1, 1, 6, 125, {0});
    builder.write_pattern(64, {});

    // Full instrument header
    builder.out.write_u32_le(tracker::xm::XM_INST_HEADER_FULL_LEN);
    builder.out.write_fixed_string("Piano", 22);
    builder.out.write_u8(0);
    builder.out.write_u16_le(1);

    builder.out.write_u32_le(tracker::xm::XM_SAMPLE_HEADER_LEN);
    for (size_t i = 0; i < 96; ++i) builder.out.write_u8(0);
    for (size_t i = 0; i < 24; ++i) builder.out.write_u16_le(0); // 12 vol pts
    for (size_t i = 0; i < 24; ++i) builder.out.write_u16_le(0); // 12 pan pts
    for (size_t i = 0; i < 14; ++i) builder.out.write_u8(0); // env props
    builder.out.write_u16_le(0); // fadeout
    builder.out.write_zeros(22); // reserved (22 bytes to complete 263-byte header)

    std::vector<int16_t> pcm16 = {1000, 2000, -3000, 4000};
    auto delta16 = tracker::xm::encode_delta_16(pcm16);

    builder.out.write_u32_le(static_cast<uint32_t>(delta16.size() * 2)); // length in bytes (8)
    builder.out.write_u32_le(2); // loop start in bytes (1 frame)
    builder.out.write_u32_le(4); // loop len in bytes (2 frames)
    builder.out.write_u8(50);
    builder.out.write_i8(-2);
    builder.out.write_u8(tracker::xm::XM_SAMPLE_16BIT | tracker::xm::XM_SAMPLE_LOOP_PINGPONG);
    builder.out.write_u8(64);
    builder.out.write_i8(-12);
    builder.out.write_u8(0);
    builder.out.write_fixed_string("Grand Piano", 22);

    for (int16_t val : delta16) {
        builder.out.write_i16_le(val);
    }

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    const auto& s = song.instruments[0].samples[0];
    REQUIRE_EQ(s.name, "Grand Piano");
    REQUIRE(s.is_16bit);
    REQUIRE(s.loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(s.length, 4);      // in 16-bit frames
    REQUIRE_EQ(s.loop_start, 1);  // 2 / 2
    REQUIRE_EQ(s.loop_length, 2); // 4 / 2
    REQUIRE_EQ(s.volume, 50);
    REQUIRE_EQ(s.finetune, -2);
    REQUIRE_EQ(s.panning, 64);
    REQUIRE_EQ(s.relative_note, -12);
    REQUIRE_EQ(s.data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(s.data16[i], pcm16[i]);
    }
}

TEST_CASE(XmReader_CorruptPatternData) {
    SyntheticXmBuilder builder;
    builder.write_header("Corrupt Pattern", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 1, 1, 0, 1, 6, 125, {0});

    // Packed size = 1, but cell requires note + inst following flag
    std::vector<uint8_t> bad_packed = {tracker::xm::XM_PACK_FLAG | tracker::xm::XM_PACK_NOTE | tracker::xm::XM_PACK_INSTRUMENT};
    builder.write_pattern(1, bad_packed);

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::CorruptPatternData);
}

TEST_CASE(XmReader_LoadFromFile) {
    SyntheticXmBuilder builder;
    builder.write_header("File Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 1, 0, 1, 6, 125, {0});
    builder.write_pattern(64, {});

    const std::string tmp_path = "test_temp_song.xm";
    {
        auto out_res = tracker::io::FileOutputStream::open(tmp_path);
        REQUIRE(out_res.is_ok());
        out_res.value().write(builder.data().data(), builder.data().size());
    }

    auto load_res = tracker::xm::XmReader::load_from_file(tmp_path);
    std::remove(tmp_path.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File Song");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(XmReader_AmigaFrequencyFlagAndFullOrderTable) {
    SyntheticXmBuilder builder;
    std::vector<uint8_t> orders(256, 0);
    for (size_t i = 0; i < 256; ++i) orders[i] = static_cast<uint8_t>(i % 5);

    // flags = 0 -> Amiga frequency table
    builder.write_header("Amiga Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         256, 10, 8, 1, 0, 0, 3, 140, orders);
    builder.write_pattern(32, {});

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE(!song.linear_frequency);
    REQUIRE_EQ(song.default_speed, 3);
    REQUIRE_EQ(song.default_bpm, 140);
    REQUIRE_EQ(song.restart_position, 10);
    REQUIRE_EQ(song.num_channels, 8);
    REQUIRE_EQ(song.order_table.size(), 256);
    REQUIRE_EQ(song.order_table[255], 255 % 5);
}

TEST_CASE(XmReader_MultiSampleInstrument) {
    SyntheticXmBuilder builder;
    builder.write_header("Multi Sample Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         1, 0, 4, 1, 1, 1, 6, 125, {0});
    builder.write_pattern(64, {});

    // Instrument with 2 samples
    builder.out.write_u32_le(tracker::xm::XM_INST_HEADER_FULL_LEN);
    builder.out.write_fixed_string("Stereo Instrument", 22);
    builder.out.write_u8(0);
    builder.out.write_u16_le(2); // 2 samples

    builder.out.write_u32_le(tracker::xm::XM_SAMPLE_HEADER_LEN);
    // Keymap: lower 48 notes -> sample 0, upper 48 notes -> sample 1
    for (size_t i = 0; i < 48; ++i) builder.out.write_u8(0);
    for (size_t i = 48; i < 96; ++i) builder.out.write_u8(1);

    for (size_t i = 0; i < 24; ++i) builder.out.write_u16_le(0);
    for (size_t i = 0; i < 24; ++i) builder.out.write_u16_le(0);
    builder.out.write_u8(16); // 16 vol points (should clamp to 12)
    builder.out.write_u8(0);  // 0 pan points
    for (size_t i = 0; i < 12; ++i) builder.out.write_u8(0); // env & vib settings
    builder.out.write_u16_le(0); // fadeout
    builder.out.write_zeros(22); // reserved

    // Sample 0: 8-bit mono, length 4
    std::vector<int8_t> pcm8 = {10, 20, 30, 40};
    auto delta8 = tracker::xm::encode_delta_8(pcm8);
    builder.out.write_u32_le(static_cast<uint32_t>(delta8.size()));
    builder.out.write_u32_le(0); // loop start
    builder.out.write_u32_le(0); // loop len
    builder.out.write_u8(60);    // volume
    builder.out.write_i8(0);     // finetune
    builder.out.write_u8(tracker::xm::XM_SAMPLE_LOOP_NONE);
    builder.out.write_u8(0);     // left panning
    builder.out.write_i8(0);     // rel note
    builder.out.write_u8(0);
    builder.out.write_fixed_string("Sample Left", 22);

    // Sample 1: 16-bit mono, length 4 frames (8 bytes)
    std::vector<int16_t> pcm16 = {500, 1000, 1500, 2000};
    auto delta16 = tracker::xm::encode_delta_16(pcm16);
    builder.out.write_u32_le(static_cast<uint32_t>(delta16.size() * 2));
    builder.out.write_u32_le(0);
    builder.out.write_u32_le(0);
    builder.out.write_u8(64);
    builder.out.write_i8(1);
    builder.out.write_u8(tracker::xm::XM_SAMPLE_16BIT);
    builder.out.write_u8(255);   // right panning
    builder.out.write_i8(12);
    builder.out.write_u8(0);
    builder.out.write_fixed_string("Sample Right", 22);

    // Write audio data for Sample 0
    builder.out.write(delta8.data(), delta8.size());
    // Write audio data for Sample 1
    for (int16_t v : delta16) builder.out.write_i16_le(v);

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& inst = res.value().instruments[0];
    REQUIRE_EQ(inst.name, "Stereo Instrument");
    REQUIRE_EQ(inst.sample_map[0], 0);
    REQUIRE_EQ(inst.sample_map[47], 0);
    REQUIRE_EQ(inst.sample_map[48], 1);
    REQUIRE_EQ(inst.sample_map[95], 1);
    REQUIRE_EQ(inst.volume_envelope.points.size(), 12); // clamped from 16 to 12
    REQUIRE_EQ(inst.samples.size(), 2);

    // Sample 0
    REQUIRE_EQ(inst.samples[0].name, "Sample Left");
    REQUIRE(!inst.samples[0].is_16bit);
    REQUIRE_EQ(inst.samples[0].panning, 0);
    REQUIRE_EQ(inst.samples[0].length, 4);
    REQUIRE_EQ(inst.samples[0].data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) REQUIRE_EQ(inst.samples[0].data8[i], pcm8[i]);

    // Sample 1
    REQUIRE_EQ(inst.samples[1].name, "Sample Right");
    REQUIRE(inst.samples[1].is_16bit);
    REQUIRE_EQ(inst.samples[1].panning, 255);
    REQUIRE_EQ(inst.samples[1].length, 4);
    REQUIRE_EQ(inst.samples[1].data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) REQUIRE_EQ(inst.samples[1].data16[i], pcm16[i]);
}

TEST_CASE(XmReader_MultiplePatternsDifferentSizes) {
    SyntheticXmBuilder builder;
    builder.write_header("Multi Pattern Song", "FastTracker v2.00", tracker::xm::XM_VERSION_104,
                         3, 0, 4, 3, 0, 1, 6, 125, {0, 1, 2});

    builder.write_pattern(32, {});
    builder.write_pattern(64, {});
    builder.write_pattern(128, {});

    auto res = tracker::xm::XmReader::load_from_memory(builder.data().data(), builder.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.patterns.size(), 3);
    REQUIRE_EQ(song.patterns[0].num_rows, 32);
    REQUIRE_EQ(song.patterns[0].num_channels, 4);
    REQUIRE_EQ(song.patterns[1].num_rows, 64);
    REQUIRE_EQ(song.patterns[1].num_channels, 4);
    REQUIRE_EQ(song.patterns[2].num_rows, 128);
    REQUIRE_EQ(song.patterns[2].num_channels, 4);
}

