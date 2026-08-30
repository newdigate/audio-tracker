#include "test_main.hpp"
#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_reader.hpp>
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(CrossFormat_ItToXmToModToIt) {
    // 1. Create IT module compatible with tri-format pipeline
    tracker::Song orig;
    orig.name = "TriFormat Cycle";
    orig.num_channels = 4;
    orig.default_speed = 6;
    orig.default_bpm = 125;
    orig.order_table = {0, 1};

    // Pattern 0
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25; // C-2
    orig.patterns[0].get_cell(0, 0).instrument = 1;
    orig.patterns[0].get_cell(0, 0).effect_type = 0x0C; // Set volume
    orig.patterns[0].get_cell(0, 0).effect_param = 0x30;

    // Pattern 1
    orig.patterns.emplace_back(64, 4);
    orig.patterns[1].get_cell(32, 2).note = 37; // C-3
    orig.patterns[1].get_cell(32, 2).instrument = 1;
    orig.patterns[1].get_cell(32, 2).effect_type = 0x0F; // Set speed
    orig.patterns[1].get_cell(32, 2).effect_param = 0x04;

    // Instrument with 8-bit looping sample
    orig.instruments.emplace_back();
    orig.instruments[0].name = "Synth Bass";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "Bass Smp";
    s.volume = 60;
    s.finetune = 0;
    s.loop_type = tracker::LoopType::Forward;
    s.loop_start = 2;
    s.loop_length = 4;
    s.data8 = {10, 20, 30, 40, 50, 60, -10, -20};
    s.length = static_cast<uint32_t>(s.data8.size());

    // 2. Export IT -> Load IT
    auto it1_res = tracker::it::ItWriter::save_to_memory(orig);
    REQUIRE(it1_res.is_ok());
    auto song_it1 = tracker::it::ItReader::load_from_memory(it1_res.value().data(), it1_res.value().size()).value();

    // 3. Export XM -> Load XM
    auto xm_res = tracker::xm::XmWriter::save_to_memory(song_it1);
    REQUIRE(xm_res.is_ok());
    auto song_xm = tracker::xm::XmReader::load_from_memory(xm_res.value().data(), xm_res.value().size()).value();

    // 4. Export MOD -> Load MOD
    auto mod_res = tracker::mod::ModWriter::save_to_memory(song_xm);
    REQUIRE(mod_res.is_ok());
    auto song_mod = tracker::mod::ModReader::load_from_memory(mod_res.value().data(), mod_res.value().size()).value();

    // 5. Export back to IT -> Final Load IT
    auto it2_res = tracker::it::ItWriter::save_to_memory(song_mod);
    REQUIRE(it2_res.is_ok());
    auto final_song = tracker::it::ItReader::load_from_memory(it2_res.value().data(), it2_res.value().size()).value();

    // Verify final IT song
    REQUIRE_EQ(final_song.name, orig.name);
    REQUIRE_EQ(final_song.num_channels, 4);
    REQUIRE_EQ(final_song.order_table.size(), 2);
    REQUIRE_EQ(final_song.patterns.size(), 2);

    const auto& c0 = final_song.patterns[0].get_cell(0, 0);
    REQUIRE_EQ(c0.note, 25);
    REQUIRE_EQ(c0.instrument, 1);
    REQUIRE_EQ(c0.effect_type, 0x0C);
    REQUIRE_EQ(c0.effect_param, 0x30);

    const auto& c1 = final_song.patterns[1].get_cell(32, 2);
    REQUIRE_EQ(c1.note, 37);
    REQUIRE_EQ(c1.instrument, 1);
    REQUIRE_EQ(c1.effect_type, 0x0F);
    REQUIRE_EQ(c1.effect_param, 0x04);

    REQUIRE(final_song.instruments.size() >= 1);
    const auto& final_s = final_song.instruments[0].samples[0];
    REQUIRE_EQ(final_s.volume, 60);
    REQUIRE(final_s.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(final_s.loop_start, 2);
    REQUIRE_EQ(final_s.loop_length, 4);
    REQUIRE_EQ(final_s.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(final_s.data8[i], s.data8[i]);
    }
}

TEST_CASE(CrossFormat_XmToItToXm) {
    // 1. Create 8-channel XM module with envelopes and 8-bit & 16-bit samples
    tracker::Song orig;
    orig.name = "XM to IT to XM";
    orig.tracker_name = "FastTracker v2.00";
    orig.num_channels = 8;
    orig.default_speed = 4;
    orig.default_bpm = 130;
    orig.linear_frequency = true;
    orig.order_table = {0, 1};

    // Pattern 0: 64 rows, 8 channels
    orig.patterns.emplace_back(64, 8);
    for (uint16_t ch = 0; ch < 8; ++ch) {
        auto& c = orig.patterns[0].get_cell(ch * 4, ch);
        c.note = static_cast<uint8_t>(25 + ch * 2);
        c.instrument = static_cast<uint8_t>(1 + (ch % 2));
        c.volume = static_cast<uint8_t>(0x10 + ch * 4);
        c.effect_type = 0x0A; // Volume slide
        c.effect_param = static_cast<uint8_t>(0x0F);
    }

    // Pattern 1: 32 rows, 8 channels
    orig.patterns.emplace_back(32, 8);
    orig.patterns[1].get_cell(0, 0).note = 49;
    orig.patterns[1].get_cell(0, 0).instrument = 1;
    orig.patterns[1].get_cell(31, 7).note = 97; // Key off

    // Instrument 1: 8-bit sample with volume envelope
    {
        orig.instruments.emplace_back();
        auto& inst = orig.instruments.back();
        inst.name = "Inst 8b";
        inst.volume_fadeout = 256;
        inst.volume_envelope.enabled = true;
        inst.volume_envelope.sustain_enabled = true;
        inst.volume_envelope.sustain_point = 1;
        inst.volume_envelope.points = {{0, 64}, {10, 48}, {30, 0}};

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Sample 8b";
        s.volume = 55;
        s.panning = 64;
        s.is_16bit = false;
        s.loop_type = tracker::LoopType::Forward;
        s.data8 = {0, 20, 40, 60, 40, 20, 0, -20, -40, -60, -40, -20};
        s.length = static_cast<uint32_t>(s.data8.size());
        s.loop_start = 2;
        s.loop_length = 8;
    }

    // Instrument 2: 16-bit sample
    {
        orig.instruments.emplace_back();
        auto& inst = orig.instruments.back();
        inst.name = "Inst 16b";
        inst.volume_fadeout = 512;

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Sample 16b";
        s.volume = 62;
        s.panning = 192;
        s.is_16bit = true;
        s.loop_type = tracker::LoopType::PingPong;
        s.data16 = {1000, 3000, 7000, 12000, 7000, 3000, -1000, -3000, -7000, -12000, -7000, -3000};
        s.length = static_cast<uint32_t>(s.data16.size());
        s.loop_start = 1;
        s.loop_length = 10;
    }

    // 2. Export XM -> Load XM
    auto xm1_res = tracker::xm::XmWriter::save_to_memory(orig);
    REQUIRE(xm1_res.is_ok());
    auto song_xm1 = tracker::xm::XmReader::load_from_memory(xm1_res.value().data(), xm1_res.value().size()).value();

    // 3. Export IT -> Load IT
    auto it_res = tracker::it::ItWriter::save_to_memory(song_xm1);
    REQUIRE(it_res.is_ok());
    auto song_it = tracker::it::ItReader::load_from_memory(it_res.value().data(), it_res.value().size()).value();

    // 4. Export XM -> Final Load XM
    auto xm2_res = tracker::xm::XmWriter::save_to_memory(song_it);
    REQUIRE(xm2_res.is_ok());
    auto final_xm = tracker::xm::XmReader::load_from_memory(xm2_res.value().data(), xm2_res.value().size()).value();

    // Verify final XM song
    REQUIRE_EQ(final_xm.name, orig.name);
    REQUIRE_EQ(final_xm.num_channels, 8);
    REQUIRE_EQ(final_xm.default_speed, 4);
    REQUIRE_EQ(final_xm.default_bpm, 130);
    REQUIRE_EQ(final_xm.patterns.size(), 2);

    for (uint16_t ch = 0; ch < 8; ++ch) {
        const auto& c = final_xm.patterns[0].get_cell(ch * 4, ch);
        REQUIRE_EQ(c.note, static_cast<uint8_t>(25 + ch * 2));
        REQUIRE_EQ(c.instrument, static_cast<uint8_t>(1 + (ch % 2)));
    }

    REQUIRE_EQ(final_xm.instruments.size(), 2);

    // Verify 8-bit sample instrument
    const auto& inst1 = final_xm.instruments[0];
    REQUIRE_EQ(inst1.name, "Inst 8b");
    REQUIRE_EQ(inst1.volume_envelope.points.size(), 3);
    REQUIRE_EQ(inst1.samples[0].data8.size(), 12);
    for (size_t i = 0; i < 12; ++i) {
        REQUIRE_EQ(inst1.samples[0].data8[i], orig.instruments[0].samples[0].data8[i]);
    }

    // Verify 16-bit sample instrument
    const auto& inst2 = final_xm.instruments[1];
    REQUIRE_EQ(inst2.name, "Inst 16b");
    REQUIRE(inst2.samples[0].is_16bit);
    REQUIRE_EQ(inst2.samples[0].data16.size(), 12);
    for (size_t i = 0; i < 12; ++i) {
        REQUIRE_EQ(inst2.samples[0].data16[i], orig.instruments[1].samples[0].data16[i]);
    }
}

TEST_CASE(CrossFormat_ModToItToMod) {
    // 1. Create MOD module
    tracker::Song orig;
    orig.name = "MOD to IT to MOD";
    orig.num_channels = 4;
    orig.order_table = {0, 1};

    // Pattern 0: 64 rows, 4 channels
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 13; // C-1
    orig.patterns[0].get_cell(0, 0).instrument = 1;
    orig.patterns[0].get_cell(0, 0).effect_type = 0x01; // Portamento Up
    orig.patterns[0].get_cell(0, 0).effect_param = 0x03;

    orig.patterns[0].get_cell(16, 1).note = 25; // C-2
    orig.patterns[0].get_cell(16, 1).instrument = 2;
    orig.patterns[0].get_cell(16, 1).effect_type = 0x0C; // Set Volume
    orig.patterns[0].get_cell(16, 1).effect_param = 0x2A;

    // Pattern 1: 64 rows, 4 channels
    orig.patterns.emplace_back(64, 4);
    orig.patterns[1].get_cell(32, 2).note = 37; // C-3
    orig.patterns[1].get_cell(32, 2).instrument = 1;
    orig.patterns[1].get_cell(32, 2).effect_type = 0x0F; // Set Speed
    orig.patterns[1].get_cell(32, 2).effect_param = 0x06;

    // Instrument 1: Looping sample
    orig.instruments.emplace_back();
    orig.instruments[0].name = "Loop Smp";
    orig.instruments[0].samples.emplace_back();
    auto& s1 = orig.instruments[0].samples.back();
    s1.name = "Bass Loop";
    s1.volume = 55;
    s1.finetune = 2;
    s1.loop_type = tracker::LoopType::Forward;
    s1.loop_start = 2;
    s1.loop_length = 4;
    s1.data8 = {10, 20, 30, 40, 50, 60, -10, -20};
    s1.length = 8;

    // Instrument 2: One-shot sample
    orig.instruments.emplace_back();
    orig.instruments[1].name = "Hit Smp";
    orig.instruments[1].samples.emplace_back();
    auto& s2 = orig.instruments[1].samples.back();
    s2.name = "Snare Hit";
    s2.volume = 64;
    s2.finetune = 0;
    s2.loop_type = tracker::LoopType::None;
    s2.data8 = {40, 20, -20, -40};
    s2.length = 4;

    // 2. Export MOD -> Load MOD
    auto mod1_res = tracker::mod::ModWriter::save_to_memory(orig);
    REQUIRE(mod1_res.is_ok());
    auto song_mod1 = tracker::mod::ModReader::load_from_memory(mod1_res.value().data(), mod1_res.value().size()).value();

    // 3. Export IT -> Load IT
    auto it_res = tracker::it::ItWriter::save_to_memory(song_mod1);
    REQUIRE(it_res.is_ok());
    auto song_it = tracker::it::ItReader::load_from_memory(it_res.value().data(), it_res.value().size()).value();

    // 4. Export MOD -> Final Load MOD
    auto mod2_res = tracker::mod::ModWriter::save_to_memory(song_it);
    REQUIRE(mod2_res.is_ok());
    auto final_mod = tracker::mod::ModReader::load_from_memory(mod2_res.value().data(), mod2_res.value().size()).value();

    // Verify final MOD song
    REQUIRE_EQ(final_mod.name, orig.name);
    REQUIRE_EQ(final_mod.num_channels, 4);
    REQUIRE_EQ(final_mod.order_table.size(), 2);
    REQUIRE_EQ(final_mod.patterns.size(), 2);

    const auto& c0 = final_mod.patterns[0].get_cell(0, 0);
    REQUIRE_EQ(c0.note, 13);
    REQUIRE_EQ(c0.instrument, 1);
    REQUIRE_EQ(c0.effect_type, 0x01);
    REQUIRE_EQ(c0.effect_param, 0x03);

    const auto& c1 = final_mod.patterns[0].get_cell(16, 1);
    REQUIRE_EQ(c1.note, 25);
    REQUIRE_EQ(c1.instrument, 2);
    REQUIRE_EQ(c1.effect_type, 0x0C);
    REQUIRE_EQ(c1.effect_param, 0x2A);

    const auto& c2 = final_mod.patterns[1].get_cell(32, 2);
    REQUIRE_EQ(c2.note, 37);
    REQUIRE_EQ(c2.instrument, 1);
    REQUIRE_EQ(c2.effect_type, 0x0F);
    REQUIRE_EQ(c2.effect_param, 0x06);

    REQUIRE(final_mod.instruments.size() >= 2);
    const auto& fs1 = final_mod.instruments[0].samples[0];
    REQUIRE_EQ(fs1.volume, 55);
    REQUIRE(fs1.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(fs1.loop_start, 2);
    REQUIRE_EQ(fs1.loop_length, 4);
    REQUIRE_EQ(fs1.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(fs1.data8[i], s1.data8[i]);
    }

    const auto& fs2 = final_mod.instruments[1].samples[0];
    REQUIRE_EQ(fs2.volume, 64);
    REQUIRE(fs2.loop_type == tracker::LoopType::None);
    REQUIRE_EQ(fs2.data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(fs2.data8[i], s2.data8[i]);
    }
}

TEST_CASE(CrossFormat_ItToModToIt) {
    tracker::Song orig;
    orig.name = "IT to MOD to IT";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25; // C-2
    orig.patterns[0].get_cell(0, 0).instrument = 1;
    orig.patterns[0].get_cell(0, 0).effect_type = 0x0C;
    orig.patterns[0].get_cell(0, 0).effect_param = 0x3F;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "Sine Inst";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "Sine Smp";
    s.volume = 64;
    s.data8 = {0, 32, 64, 32, 0, -32, -64, -32};
    s.length = 8;

    // Export IT -> Load IT
    auto it1_res = tracker::it::ItWriter::save_to_memory(orig);
    REQUIRE(it1_res.is_ok());
    auto song_it1 = tracker::it::ItReader::load_from_memory(it1_res.value().data(), it1_res.value().size()).value();

    // Export MOD -> Load MOD
    auto mod_res = tracker::mod::ModWriter::save_to_memory(song_it1);
    REQUIRE(mod_res.is_ok());
    auto song_mod = tracker::mod::ModReader::load_from_memory(mod_res.value().data(), mod_res.value().size()).value();

    // Export IT -> Final Load IT
    auto it2_res = tracker::it::ItWriter::save_to_memory(song_mod);
    REQUIRE(it2_res.is_ok());
    auto final_it = tracker::it::ItReader::load_from_memory(it2_res.value().data(), it2_res.value().size()).value();

    REQUIRE_EQ(final_it.name, orig.name);
    REQUIRE_EQ(final_it.num_channels, 4);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).effect_type, 0x0C);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).effect_param, 0x3F);
    REQUIRE_EQ(final_it.instruments[0].samples[0].data8.size(), 8);
}

TEST_CASE(CrossFormat_TriFormatFileRoundTrips) {
    tracker::Song orig;
    orig.name = "TriFile Cycle";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25;
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "File Inst";
    orig.instruments[0].samples.emplace_back();
    orig.instruments[0].samples[0].name = "File Smp";
    orig.instruments[0].samples[0].data8 = {10, 20, 30, 40, -10, -20, -30, -40};
    orig.instruments[0].samples[0].length = 8;

    const std::string tmp_it = "test_cross_tri_format.it";
    const std::string tmp_xm = "test_cross_tri_format.xm";
    const std::string tmp_mod = "test_cross_tri_format.mod";
    const std::string tmp_it2 = "test_cross_tri_format2.it";

    // 1. Save IT file
    REQUIRE(tracker::it::ItWriter::save_to_file(orig, tmp_it).is_ok());

    // 2. Load IT file -> Save XM file
    auto it_song = tracker::it::ItReader::load_from_file(tmp_it).value();
    std::remove(tmp_it.c_str());
    REQUIRE(tracker::xm::XmWriter::save_to_file(it_song, tmp_xm).is_ok());

    // 3. Load XM file -> Save MOD file
    auto xm_song = tracker::xm::XmReader::load_from_file(tmp_xm).value();
    std::remove(tmp_xm.c_str());
    REQUIRE(tracker::mod::ModWriter::save_to_file(xm_song, tmp_mod).is_ok());

    // 4. Load MOD file -> Save IT file
    auto mod_song = tracker::mod::ModReader::load_from_file(tmp_mod).value();
    std::remove(tmp_mod.c_str());
    REQUIRE(tracker::it::ItWriter::save_to_file(mod_song, tmp_it2).is_ok());

    // 5. Final Load from IT file
    auto final_it = tracker::it::ItReader::load_from_file(tmp_it2).value();
    std::remove(tmp_it2.c_str());

    REQUIRE_EQ(final_it.name, orig.name);
    REQUIRE_EQ(final_it.num_channels, 4);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(final_it.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(final_it.instruments[0].samples[0].data8.size(), 8);
}

TEST_CASE(CrossFormat_16BitSampleDowngradeAndPreservation_ItModXm) {
    tracker::Song orig;
    orig.name = "16b TriCross";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);
    orig.patterns[0].get_cell(0, 0).note = 25;
    orig.patterns[0].get_cell(0, 0).instrument = 1;

    orig.instruments.emplace_back();
    orig.instruments[0].name = "16bit Inst";
    orig.instruments[0].samples.emplace_back();
    auto& s = orig.instruments[0].samples.back();
    s.name = "HiFi Smp";
    s.is_16bit = true;
    s.volume = 64;
    s.data16 = {0x0000, 0x2000, 0x4000, 0x2000, 0x0000, static_cast<int16_t>(0xE000), static_cast<int16_t>(0xC000), static_cast<int16_t>(0xE000)};
    s.length = 8;

    // 1. Save IT -> Load IT -> Verify 16-bit
    auto it1 = tracker::it::ItWriter::save_to_memory(orig).value();
    auto song_it1 = tracker::it::ItReader::load_from_memory(it1.data(), it1.size()).value();
    REQUIRE(song_it1.instruments[0].samples[0].is_16bit);

    // 2. Export to XM -> Verify 16-bit preserved
    auto xm1 = tracker::xm::XmWriter::save_to_memory(song_it1).value();
    auto song_xm = tracker::xm::XmReader::load_from_memory(xm1.data(), xm1.size()).value();
    REQUIRE(song_xm.instruments[0].samples[0].is_16bit);

    // 3. Export to MOD -> Verify downsampled to 8-bit
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

    // 4. Export back to IT -> Verify saved as 8-bit
    auto it2 = tracker::it::ItWriter::save_to_memory(song_mod).value();
    auto final_it = tracker::it::ItReader::load_from_memory(it2.data(), it2.size()).value();
    const auto& s_final = final_it.instruments[0].samples[0];
    REQUIRE(!s_final.is_16bit);
    REQUIRE_EQ(s_final.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(s_final.data8[i], s_mod.data8[i]);
    }
}

TEST_CASE(CrossFormat_CommonEffectsPreservation_TriFormat) {
    tracker::Song orig;
    orig.name = "Tri FX Match";
    orig.num_channels = 4;
    orig.order_table = {0};
    orig.patterns.emplace_back(64, 4);

    // Common standard tracker effects supported across IT, XM, and MOD:
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

    // Convert: Orig (IT) -> XM -> MOD -> IT -> XM -> MOD
    auto it1 = tracker::it::ItWriter::save_to_memory(orig).value();
    auto s_it1 = tracker::it::ItReader::load_from_memory(it1.data(), it1.size()).value();

    auto xm1 = tracker::xm::XmWriter::save_to_memory(s_it1).value();
    auto s_xm1 = tracker::xm::XmReader::load_from_memory(xm1.data(), xm1.size()).value();

    auto mod1 = tracker::mod::ModWriter::save_to_memory(s_xm1).value();
    auto s_mod1 = tracker::mod::ModReader::load_from_memory(mod1.data(), mod1.size()).value();

    auto it2 = tracker::it::ItWriter::save_to_memory(s_mod1).value();
    auto s_it2 = tracker::it::ItReader::load_from_memory(it2.data(), it2.size()).value();

    auto xm2 = tracker::xm::XmWriter::save_to_memory(s_it2).value();
    auto s_xm2 = tracker::xm::XmReader::load_from_memory(xm2.data(), xm2.size()).value();

    auto mod2 = tracker::mod::ModWriter::save_to_memory(s_xm2).value();
    auto final_mod = tracker::mod::ModReader::load_from_memory(mod2.data(), mod2.size()).value();

    for (size_t i = 0; i < effects.size(); ++i) {
        const auto& c = final_mod.patterns[0].get_cell(static_cast<uint16_t>(i), 0);
        REQUIRE_EQ(c.note, static_cast<uint8_t>(13 + i * 2));
        REQUIRE_EQ(c.instrument, 1);
        REQUIRE_EQ(c.effect_type, effects[i].first);
        REQUIRE_EQ(c.effect_param, effects[i].second);
    }
}
