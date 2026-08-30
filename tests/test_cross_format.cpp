#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>
#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_reader.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(CrossFormat_XmToModToXm) {
    // 1. Create Song
    tracker::Song orig;
    orig.name = "Cross Platform";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25; // C-2
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "Sine";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "Sine 8bit";
    s.volume = 64;
    s.data8 = {0, 32, 64, 32, 0, -32, -64, -32};
    s.length = static_cast<uint32_t>(s.data8.size());

    // 2. Export to XM bytes
    auto xm_bytes_res = tracker::xm::XmWriter::save_to_memory(orig);
    REQUIRE(xm_bytes_res.is_ok());

    // 3. Load XM -> Export to MOD
    auto xm_song = tracker::xm::XmReader::load_from_memory(xm_bytes_res.value().data(), xm_bytes_res.value().size()).value();
    auto mod_bytes_res = tracker::mod::ModWriter::save_to_memory(xm_song);
    REQUIRE(mod_bytes_res.is_ok());

    // 4. Load MOD -> Export to XM
    auto mod_song = tracker::mod::ModReader::load_from_memory(mod_bytes_res.value().data(), mod_bytes_res.value().size()).value();
    auto roundtrip_xm_res = tracker::xm::XmWriter::save_to_memory(mod_song);
    REQUIRE(roundtrip_xm_res.is_ok());

    // 5. Final Load from XM and assert data
    auto final_song = tracker::xm::XmReader::load_from_memory(roundtrip_xm_res.value().data(), roundtrip_xm_res.value().size()).value();
    REQUIRE_EQ(final_song.name, orig.name);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(final_song.instruments[0].samples[0].data8.size(), s.data8.size());
}

TEST_CASE(CrossFormat_ModToXmToMod) {
    // 1. Create MOD-compatible Song
    tracker::Song orig;
    orig.name = "AmigaToPC";
    orig.num_channels = 4;
    orig.order_table = {0, 1};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 13; // C-1
    orig.patterns[0].get_cell(0, 0).instrument = 1;
    orig.patterns[0].get_cell(0, 0).effect_type = 0x0C; // Set volume
    orig.patterns[0].get_cell(0, 0).effect_param = 0x28;

    orig.patterns.emplace_back(64, 4);
    orig.patterns[1].get_cell(32, 2).note = 37; // C-3
    orig.patterns[1].get_cell(32, 2).instrument = 2;
    orig.patterns[1].get_cell(32, 2).effect_type = 0x0F; // Set speed
    orig.patterns[1].get_cell(32, 2).effect_param = 0x04;

    // Instrument 1: Looping sample
    orig.instruments.emplace_back();
    orig.instruments[0].name = "BassSynth";
    orig.instruments[0].samples.emplace_back();
    auto& s1 = orig.instruments[0].samples.back();
    s1.name = "Bass";
    s1.volume = 60;
    s1.finetune = 3;
    s1.loop_type = tracker::LoopType::Forward;
    s1.loop_start = 2;
    s1.loop_length = 4;
    s1.data8 = {10, 20, 30, 40, 50, 60, -10, -20};
    s1.length = 8;

    // Instrument 2: One-shot sample
    orig.instruments.emplace_back();
    orig.instruments[1].name = "Clap";
    orig.instruments[1].samples.emplace_back();
    auto& s2 = orig.instruments[1].samples.back();
    s2.name = "ClapHit";
    s2.volume = 64;
    s2.finetune = 0;
    s2.loop_type = tracker::LoopType::None;
    s2.data8 = {50, 25, -25, -50};
    s2.length = 4;

    // 2. Export MOD bytes
    auto mod_bytes_res = tracker::mod::ModWriter::save_to_memory(orig);
    REQUIRE(mod_bytes_res.is_ok());

    // 3. Load MOD -> Export XM
    auto mod_song1 = tracker::mod::ModReader::load_from_memory(mod_bytes_res.value().data(), mod_bytes_res.value().size()).value();
    auto xm_bytes_res = tracker::xm::XmWriter::save_to_memory(mod_song1);
    REQUIRE(xm_bytes_res.is_ok());

    // 4. Load XM -> Export MOD
    auto xm_song1 = tracker::xm::XmReader::load_from_memory(xm_bytes_res.value().data(), xm_bytes_res.value().size()).value();
    auto mod_bytes_res2 = tracker::mod::ModWriter::save_to_memory(xm_song1);
    REQUIRE(mod_bytes_res2.is_ok());

    // 5. Final Load from MOD and assert full fidelity
    auto final_mod = tracker::mod::ModReader::load_from_memory(mod_bytes_res2.value().data(), mod_bytes_res2.value().size()).value();
    REQUIRE_EQ(final_mod.name, orig.name);
    REQUIRE_EQ(final_mod.num_channels, 4);
    REQUIRE_EQ(final_mod.order_table.size(), 2);
    REQUIRE_EQ(final_mod.patterns.size(), 2);

    // Verify cell data
    const auto& c0 = final_mod.patterns[0].get_cell(0, 0);
    REQUIRE_EQ(c0.note, 13);
    REQUIRE_EQ(c0.instrument, 1);
    REQUIRE_EQ(c0.effect_type, 0x0C);
    REQUIRE_EQ(c0.effect_param, 0x28);

    const auto& c1 = final_mod.patterns[1].get_cell(32, 2);
    REQUIRE_EQ(c1.note, 37);
    REQUIRE_EQ(c1.instrument, 2);
    REQUIRE_EQ(c1.effect_type, 0x0F);
    REQUIRE_EQ(c1.effect_param, 0x04);

    // Verify samples
    REQUIRE(final_mod.instruments.size() >= 2);
    const auto& fs1 = final_mod.instruments[0].samples[0];
    REQUIRE_EQ(fs1.name, s1.name);
    REQUIRE_EQ(fs1.volume, 60);
    REQUIRE_EQ(fs1.finetune, 3);
    REQUIRE(fs1.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(fs1.loop_start, 2);
    REQUIRE_EQ(fs1.loop_length, 4);
    REQUIRE_EQ(fs1.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(fs1.data8[i], s1.data8[i]);
    }

    const auto& fs2 = final_mod.instruments[1].samples[0];
    REQUIRE_EQ(fs2.name, s2.name);
    REQUIRE_EQ(fs2.volume, 64);
    REQUIRE_EQ(fs2.finetune, 0);
    REQUIRE(fs2.loop_type == tracker::LoopType::None);
    REQUIRE_EQ(fs2.data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(fs2.data8[i], s2.data8[i]);
    }
}

TEST_CASE(CrossFormat_EffectsPreservation) {
    tracker::Song orig;
    orig.name = "Effects Match";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);

    // Place various standard tracker effects supported by both formats
    // Portamento Up (0x01), Portamento Down (0x02), Tone Portamento (0x03), Vibrato (0x04),
    // Volume Slide (0x0A), Position Jump (0x0B), Set Volume (0x0C), Pattern Break (0x0D), Set Speed (0x0F)
    const std::vector<std::pair<uint8_t, uint8_t>> effects = {
        {0x01, 0x05},
        {0x02, 0x0A},
        {0x03, 0x12},
        {0x04, 0x34},
        {0x0A, 0x0F},
        {0x0B, 0x02},
        {0x0C, 0x3F},
        {0x0D, 0x00},
        {0x0F, 0x06}
    };

    for (size_t i = 0; i < effects.size(); ++i) {
        auto& cell = orig.patterns[0].get_cell(static_cast<uint16_t>(i), 0);
        cell.note = static_cast<uint8_t>(13 + i * 2);
        cell.instrument = 1;
        cell.effect_type = effects[i].first;
        cell.effect_param = effects[i].second;
    }

    orig.instruments.emplace_back();
    orig.instruments[0].name = "TestInst";
    orig.instruments[0].samples.emplace_back();
    orig.instruments[0].samples[0].name = "Sample";
    orig.instruments[0].samples[0].data8 = {10, -10};
    orig.instruments[0].samples[0].length = 2;

    // Convert: Orig -> MOD -> XM -> MOD
    auto mod1_bytes = tracker::mod::ModWriter::save_to_memory(orig).value();
    auto mod1_song = tracker::mod::ModReader::load_from_memory(mod1_bytes.data(), mod1_bytes.size()).value();
    auto xm_bytes = tracker::xm::XmWriter::save_to_memory(mod1_song).value();
    auto xm_song = tracker::xm::XmReader::load_from_memory(xm_bytes.data(), xm_bytes.size()).value();
    auto mod2_bytes = tracker::mod::ModWriter::save_to_memory(xm_song).value();
    auto final_mod = tracker::mod::ModReader::load_from_memory(mod2_bytes.data(), mod2_bytes.size()).value();

    for (size_t i = 0; i < effects.size(); ++i) {
        const auto& c = final_mod.patterns[0].get_cell(static_cast<uint16_t>(i), 0);
        REQUIRE_EQ(c.note, static_cast<uint8_t>(13 + i * 2));
        REQUIRE_EQ(c.instrument, 1);
        REQUIRE_EQ(c.effect_type, effects[i].first);
        REQUIRE_EQ(c.effect_param, effects[i].second);
    }
}

TEST_CASE(CrossFormat_8Channel_XmModXm) {
    tracker::Song orig;
    orig.name = "8Ch Cross";
    orig.num_channels = 8;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 8);

    for (uint16_t ch = 0; ch < 8; ++ch) {
        auto& cell = orig.patterns[0].get_cell(ch * 4, ch);
        cell.note = static_cast<uint8_t>(20 + ch * 2);
        cell.instrument = 1;
        cell.effect_type = 0x0C;
        cell.effect_param = static_cast<uint8_t>(0x10 * ch);
    }

    orig.instruments.emplace_back();
    orig.instruments[0].name = "Inst 8Ch";
    orig.instruments[0].samples.emplace_back();
    orig.instruments[0].samples[0].name = "Wave";
    orig.instruments[0].samples[0].data8 = {1, 2, 3, 4, -1, -2, -3, -4};
    orig.instruments[0].samples[0].length = 8;

    // Save to XM
    auto xm1 = tracker::xm::XmWriter::save_to_memory(orig).value();
    auto song_from_xm = tracker::xm::XmReader::load_from_memory(xm1.data(), xm1.size()).value();

    // Export to MOD (8CHN)
    auto mod1 = tracker::mod::ModWriter::save_to_memory(song_from_xm).value();
    auto song_from_mod = tracker::mod::ModReader::load_from_memory(mod1.data(), mod1.size()).value();

    REQUIRE_EQ(song_from_mod.num_channels, 8);
    for (uint16_t ch = 0; ch < 8; ++ch) {
        const auto& cell = song_from_mod.patterns[0].get_cell(ch * 4, ch);
        REQUIRE_EQ(cell.note, static_cast<uint8_t>(20 + ch * 2));
        REQUIRE_EQ(cell.instrument, 1);
        REQUIRE_EQ(cell.effect_type, 0x0C);
        REQUIRE_EQ(cell.effect_param, static_cast<uint8_t>(0x10 * ch));
    }

    // Export back to XM
    auto xm2 = tracker::xm::XmWriter::save_to_memory(song_from_mod).value();
    auto final_xm = tracker::xm::XmReader::load_from_memory(xm2.data(), xm2.size()).value();

    REQUIRE_EQ(final_xm.num_channels, 8);
    for (uint16_t ch = 0; ch < 8; ++ch) {
        const auto& cell = final_xm.patterns[0].get_cell(ch * 4, ch);
        REQUIRE_EQ(cell.note, static_cast<uint8_t>(20 + ch * 2));
        REQUIRE_EQ(cell.instrument, 1);
    }
}

TEST_CASE(CrossFormat_16BitSampleDowngradeAndPreservation) {
    tracker::Song orig;
    orig.name = "HiFi to LoFi";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25;
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "16bit Sine";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "Sine 16";
    s.is_16bit = true;
    s.volume = 64;
    // High-precision 16-bit audio
    s.data16 = {0x0000, 0x2000, 0x4000, 0x2000, 0x0000, static_cast<int16_t>(0xE000), static_cast<int16_t>(0xC000), static_cast<int16_t>(0xE000)};
    s.length = 8;

    // 1. Save to XM (preserves 16-bit)
    auto xm1 = tracker::xm::XmWriter::save_to_memory(orig).value();
    auto song_xm = tracker::xm::XmReader::load_from_memory(xm1.data(), xm1.size()).value();
    REQUIRE(song_xm.instruments[0].samples[0].is_16bit);

    // 2. Export to MOD (downsampled to 8-bit)
    auto mod1 = tracker::mod::ModWriter::save_to_memory(song_xm).value();
    auto song_mod = tracker::mod::ModReader::load_from_memory(mod1.data(), mod1.size()).value();
    const auto& s_mod = song_mod.instruments[0].samples[0];
    REQUIRE(!s_mod.is_16bit);
    REQUIRE_EQ(s_mod.data8.size(), 8);
    REQUIRE_EQ(s_mod.data8[0], 0x00);
    REQUIRE_EQ(s_mod.data8[1], 0x20);
    REQUIRE_EQ(s_mod.data8[2], 0x40);
    REQUIRE_EQ(s_mod.data8[3], 0x20);
    REQUIRE_EQ(s_mod.data8[4], 0x00);
    REQUIRE_EQ(s_mod.data8[5], static_cast<int8_t>(0xE0));
    REQUIRE_EQ(s_mod.data8[6], static_cast<int8_t>(0xC0));
    REQUIRE_EQ(s_mod.data8[7], static_cast<int8_t>(0xE0));

    // 3. Export back to XM (saved as 8-bit sample)
    auto xm2 = tracker::xm::XmWriter::save_to_memory(song_mod).value();
    auto final_xm = tracker::xm::XmReader::load_from_memory(xm2.data(), xm2.size()).value();
    const auto& s_final = final_xm.instruments[0].samples[0];
    REQUIRE(!s_final.is_16bit);
    REQUIRE_EQ(s_final.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(s_final.data8[i], s_mod.data8[i]);
    }
}

TEST_CASE(CrossFormat_FileRoundTrips) {
    tracker::Song orig;
    orig.name = "File Cross Song";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25;
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "Inst File";
    orig.instruments[0].samples.emplace_back();
    orig.instruments[0].samples[0].name = "Smp File";
    orig.instruments[0].samples[0].data8 = {10, 20, 30, 40, -10, -20, -30, -40};
    orig.instruments[0].samples[0].length = 8;

    const std::string tmp_xm = "test_cross_format_temp.xm";
    const std::string tmp_mod = "test_cross_format_temp.mod";

    // 1. Save to XM file
    REQUIRE(tracker::xm::XmWriter::save_to_file(orig, tmp_xm).is_ok());

    // 2. Load from XM file -> Save to MOD file
    auto xm_song = tracker::xm::XmReader::load_from_file(tmp_xm).value();
    std::remove(tmp_xm.c_str());

    REQUIRE(tracker::mod::ModWriter::save_to_file(xm_song, tmp_mod).is_ok());

    // 3. Load from MOD file -> Verify
    auto mod_song = tracker::mod::ModReader::load_from_file(tmp_mod).value();
    std::remove(tmp_mod.c_str());

    REQUIRE_EQ(mod_song.name, orig.name);
    REQUIRE_EQ(mod_song.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(mod_song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(mod_song.instruments[0].samples[0].data8.size(), 8);
}

TEST_CASE(CrossFormat_ItToXmToIt) {
    tracker::Song orig;
    orig.name = "IT-XM-IT";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 49;
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    tracker::Instrument inst;
    inst.name = "CrossInst";
    tracker::Sample s;
    s.name = "CrossSmp";
    s.data8 = {10, 20, 30, 20, 10, 0, -10, -20};
    s.length = 8;
    s.volume = 64;
    inst.samples.push_back(std::move(s));
    orig.instruments.push_back(std::move(inst));

    // Save to IT
    auto it_res = tracker::it::ItWriter::save_to_memory(orig);
    REQUIRE(it_res.is_ok());

    // Load IT -> Save to XM
    auto it_song = tracker::it::ItReader::load_from_memory(it_res.value().data(), it_res.value().size()).value();
    auto xm_res = tracker::xm::XmWriter::save_to_memory(it_song);
    REQUIRE(xm_res.is_ok());

    // Load XM -> Save to IT
    auto xm_song = tracker::xm::XmReader::load_from_memory(xm_res.value().data(), xm_res.value().size()).value();
    auto it_res2 = tracker::it::ItWriter::save_to_memory(xm_song);
    REQUIRE(it_res2.is_ok());

    // Final load from IT
    auto final_song = tracker::it::ItReader::load_from_memory(it_res2.value().data(), it_res2.value().size()).value();
    REQUIRE_EQ(final_song.name, "IT-XM-IT");
    REQUIRE_EQ(final_song.num_channels, 4);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).note, 49);
    REQUIRE_EQ(final_song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(final_song.instruments[0].samples[0].data8.size(), 8);
}

