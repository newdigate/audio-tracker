#include "test_main.hpp"
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(ModWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Export Mod";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    auto res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    const auto& bytes = res.value();
    REQUIRE(bytes.size() >= 1084 + (64 * 4 * 4));

    // Verify signature at offset 1080
    std::string tag(reinterpret_cast<const char*>(bytes.data() + 1080), 4);
    REQUIRE_EQ(tag, "M.K.");
}

TEST_CASE(ModWriter_ChannelSignatures) {
    auto test_channels = [](uint16_t num_channels, const std::string& expected_sig) {
        tracker::Song song;
        song.name = "Sig Test";
        song.num_channels = num_channels;
        song.order_table = {0};
        song.patterns.emplace_back(64, num_channels);

        auto res = tracker::mod::ModWriter::save_to_memory(song);
        REQUIRE(res.is_ok());

        const auto& bytes = res.value();
        std::string tag(reinterpret_cast<const char*>(bytes.data() + 1080), 4);
        REQUIRE_EQ(tag, expected_sig);

        // Verify ModReader can read it back with the correct channel count
        auto read_res = tracker::mod::ModReader::load_from_memory(bytes.data(), bytes.size());
        REQUIRE(read_res.is_ok());
        REQUIRE_EQ(read_res.value().num_channels, num_channels);
    };

    test_channels(4, "M.K.");
    test_channels(6, "6CHN");
    test_channels(8, "8CHN");
    test_channels(5, "5CHN");
    test_channels(7, "7CHN");
    test_channels(16, "16CN");
    test_channels(32, "32CN");
}

TEST_CASE(ModWriter_RoundTrip_PatternsAndCells) {
    tracker::Song song;
    song.name = "Pattern Test";
    song.num_channels = 4;
    song.order_table = {0, 1};
    song.restart_position = 1;

    tracker::Pattern pat0(64, 4);
    // Row 0, Ch 0: Note 25 (C-2), Instrument 1, Effect 0x0C (Set Volume), Param 0x30
    pat0.get_cell(0, 0).note = 25;
    pat0.get_cell(0, 0).instrument = 1;
    pat0.get_cell(0, 0).effect_type = 0x0C;
    pat0.get_cell(0, 0).effect_param = 0x30;

    // Row 10, Ch 2: Note 37 (C-3), Instrument 2, Effect 0x0F (Set Speed), Param 0x04
    pat0.get_cell(10, 2).note = 37;
    pat0.get_cell(10, 2).instrument = 2;
    pat0.get_cell(10, 2).effect_type = 0x0F;
    pat0.get_cell(10, 2).effect_param = 0x04;

    tracker::Pattern pat1(64, 4);
    // Row 63, Ch 3: Note 49 (C-4), Instrument 31, Effect 0x0B (Position Jump), Param 0x00
    pat1.get_cell(63, 3).note = 49;
    pat1.get_cell(63, 3).instrument = 31;
    pat1.get_cell(63, 3).effect_type = 0x0B;
    pat1.get_cell(63, 3).effect_param = 0x00;

    song.patterns.push_back(std::move(pat0));
    song.patterns.push_back(std::move(pat1));

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, "Pattern Test");
    REQUIRE_EQ(loaded.num_channels, 4);
    REQUIRE_EQ(loaded.order_table.size(), 2);
    REQUIRE_EQ(loaded.order_table[0], 0);
    REQUIRE_EQ(loaded.order_table[1], 1);
    REQUIRE_EQ(loaded.restart_position, 1);
    REQUIRE_EQ(loaded.patterns.size(), 2);

    // Verify cell at (0, 0)
    const auto& c0 = loaded.patterns[0].get_cell(0, 0);
    REQUIRE_EQ(c0.note, 25);
    REQUIRE_EQ(c0.instrument, 1);
    REQUIRE_EQ(c0.effect_type, 0x0C);
    REQUIRE_EQ(c0.effect_param, 0x30);

    // Verify cell at (10, 2)
    const auto& c1 = loaded.patterns[0].get_cell(10, 2);
    REQUIRE_EQ(c1.note, 37);
    REQUIRE_EQ(c1.instrument, 2);
    REQUIRE_EQ(c1.effect_type, 0x0F);
    REQUIRE_EQ(c1.effect_param, 0x04);

    // Verify cell at (63, 3) in pattern 1
    const auto& c2 = loaded.patterns[1].get_cell(63, 3);
    REQUIRE_EQ(c2.note, 49);
    REQUIRE_EQ(c2.instrument, 31);
    REQUIRE_EQ(c2.effect_type, 0x0B);
    REQUIRE_EQ(c2.effect_param, 0x00);
}

TEST_CASE(ModWriter_RoundTrip_Samples8Bit) {
    tracker::Song song;
    song.name = "Sample MOD";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    // Sample 0: Looping sample
    tracker::Instrument inst0;
    inst0.name = "Synth Lead";
    tracker::Sample s0;
    s0.name = "Synth Lead";
    s0.length = 8;
    s0.volume = 55;
    s0.finetune = 3;
    s0.loop_type = tracker::LoopType::Forward;
    s0.loop_start = 2;
    s0.loop_length = 4;
    s0.is_16bit = false;
    s0.data8 = {10, 20, 30, 40, 50, 60, 70, -80};
    inst0.samples.push_back(s0);
    song.instruments.push_back(inst0);

    // Sample 1: Non-looping sample with negative finetune
    tracker::Instrument inst1;
    inst1.name = "Kick Drum";
    tracker::Sample s1;
    s1.name = "Kick Drum";
    s1.length = 4;
    s1.volume = 64;
    s1.finetune = -4;
    s1.loop_type = tracker::LoopType::None;
    s1.is_16bit = false;
    s1.data8 = {-10, -20, 30, 40};
    inst1.samples.push_back(s1);
    song.instruments.push_back(inst1);

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE(loaded.instruments.size() >= 2);

    // Sample 0 checks
    const auto& loaded_s0 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(loaded_s0.name, "Synth Lead");
    REQUIRE_EQ(loaded_s0.length, 8);
    REQUIRE_EQ(loaded_s0.volume, 55);
    REQUIRE_EQ(loaded_s0.finetune, 3);
    REQUIRE(loaded_s0.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(loaded_s0.loop_start, 2);
    REQUIRE_EQ(loaded_s0.loop_length, 4);
    REQUIRE_EQ(loaded_s0.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(loaded_s0.data8[i], s0.data8[i]);
    }

    // Sample 1 checks
    const auto& loaded_s1 = loaded.instruments[1].samples[0];
    REQUIRE_EQ(loaded_s1.name, "Kick Drum");
    REQUIRE_EQ(loaded_s1.length, 4);
    REQUIRE_EQ(loaded_s1.volume, 64);
    REQUIRE_EQ(loaded_s1.finetune, -4);
    REQUIRE(loaded_s1.loop_type == tracker::LoopType::None);
    REQUIRE_EQ(loaded_s1.data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(loaded_s1.data8[i], s1.data8[i]);
    }
}

TEST_CASE(ModWriter_16BitSampleDownsampling) {
    tracker::Song song;
    song.name = "16bit Downsample";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "HiFi Sample";
    tracker::Sample s;
    s.name = "HiFi Sample";
    s.is_16bit = true;
    s.length = 4;
    s.volume = 60;
    s.finetune = 0;
    s.loop_type = tracker::LoopType::None;
    // 0x1200 -> 0x12 (18), 0x7F00 -> 0x7F (127), -0x2000 (-8192) -> -32 (0xE0), -0x8000 (-32768) -> -128
    s.data16 = {0x1200, 0x7F00, static_cast<int16_t>(0xE000), static_cast<int16_t>(0x8000)};
    inst.samples.push_back(s);
    song.instruments.push_back(inst);

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    const auto& loaded_s = loaded.instruments[0].samples[0];
    REQUIRE(!loaded_s.is_16bit);
    REQUIRE_EQ(loaded_s.length, 4);
    REQUIRE_EQ(loaded_s.data8.size(), 4);
    REQUIRE_EQ(loaded_s.data8[0], 0x12);
    REQUIRE_EQ(loaded_s.data8[1], 0x7F);
    REQUIRE_EQ(loaded_s.data8[2], static_cast<int8_t>(0xE0));
    REQUIRE_EQ(loaded_s.data8[3], static_cast<int8_t>(0x80));
}

TEST_CASE(ModWriter_OrderTableEdgeCases) {
    tracker::Song song;
    song.name = "Empty Orders";
    song.num_channels = 4;
    song.order_table = {}; // empty order table
    song.patterns.emplace_back(64, 4);

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    // When order table is empty, writer should default song length to 1
    const auto& bytes = save_res.value();
    REQUIRE_EQ(bytes[950], 1);

    // Large order table (> 128 entries) should be clamped to 128
    for (size_t i = 0; i < 200; ++i) {
        song.order_table.push_back(static_cast<uint8_t>(i % 10));
    }
    auto save_res2 = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res2.is_ok());
    REQUIRE_EQ(save_res2.value()[950], 128);
}

TEST_CASE(ModWriter_SaveToFile_And_LoadFromFile) {
    tracker::Song song;
    song.name = "File IO Test";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    const std::string tmp_file = "test_temp_mod_writer.mod";
    auto status = tracker::mod::ModWriter::save_to_file(song, tmp_file);
    REQUIRE(status.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_file(tmp_file);
    std::remove(tmp_file.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File IO Test");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(ModWriter_SaveToFile_Error) {
    tracker::Song song;
    song.name = "Bad Path";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    auto status = tracker::mod::ModWriter::save_to_file(song, "/non_existent_directory/file.mod");
    REQUIRE(!status.is_ok());
    REQUIRE_EQ(status.code, tracker::ErrorCode::IoError);
}

TEST_CASE(ModWriter_DirectStreamSave) {
    tracker::Song song;
    song.name = "Stream Save";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::io::MemoryOutputStream stream;
    auto status = tracker::mod::ModWriter::save(song, stream);
    REQUIRE(status.is_ok());
    REQUIRE(stream.data().size() >= 1084 + (64 * 4 * 4));

    auto load_res = tracker::mod::ModReader::load_from_memory(stream.data().data(), stream.data().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Stream Save");
}
