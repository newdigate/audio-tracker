#include "test_main.hpp"
#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_reader.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(ModRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Amiga Classic";
    original.num_channels = 4;
    original.order_table = {0, 1, 0};

    // Pattern 0
    original.patterns.emplace_back(64, 4);
    original.patterns[0].get_cell(0, 0).note = 13; // C-1
    original.patterns[0].get_cell(0, 0).instrument = 1;
    original.patterns[0].get_cell(0, 0).effect_type = 0x0C; // Set volume
    original.patterns[0].get_cell(0, 0).effect_param = 0x30;

    // Pattern 1
    original.patterns.emplace_back(64, 4);
    original.patterns[1].get_cell(10, 2).note = 25; // C-2
    original.patterns[1].get_cell(10, 2).instrument = 2;

    // Instrument 1: Sample with loop
    original.instruments.emplace_back();
    auto& inst1 = original.instruments.back();
    inst1.name = "Bass";
    inst1.samples.emplace_back();
    auto& s1 = inst1.samples.back();
    s1.name = "Bass Sample";
    s1.volume = 58;
    s1.finetune = 2;
    s1.loop_type = tracker::LoopType::Forward;
    s1.data8 = {0, 10, 20, 10, 0, -10, -20, -10};
    s1.length = static_cast<uint32_t>(s1.data8.size());
    s1.loop_start = 2;
    s1.loop_length = 4;

    // Instrument 2: Unlooped sample
    original.instruments.emplace_back();
    auto& inst2 = original.instruments.back();
    inst2.name = "Snare";
    inst2.samples.emplace_back();
    auto& s2 = inst2.samples.back();
    s2.name = "Snare Hit";
    s2.volume = 64;
    s2.data8 = {30, -30, 20, -20, 10, -10};
    s2.length = static_cast<uint32_t>(s2.data8.size());

    // Save to MOD memory buffer
    auto save_res = tracker::mod::ModWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& mod_bytes = save_res.value();
    REQUIRE(!mod_bytes.empty());

    // Load back from MOD memory buffer
    auto load_res = tracker::mod::ModReader::load_from_memory(mod_bytes.data(), mod_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.num_channels, 4);
    REQUIRE_EQ(loaded.order_table.size(), 3);
    REQUIRE_EQ(loaded.patterns.size(), 2);

    // Verify cell contents
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 13);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x0C);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x30);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 2).note, 25);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 2).instrument, 2);

    // Verify sample 1
    const auto& ls1 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls1.name, s1.name);
    REQUIRE_EQ(ls1.volume, 58);
    REQUIRE_EQ(ls1.finetune, 2);
    REQUIRE(ls1.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(ls1.loop_start, 2);
    REQUIRE_EQ(ls1.loop_length, 4);
    REQUIRE_EQ(ls1.data8.size(), s1.data8.size());
    for (size_t i = 0; i < s1.data8.size(); ++i) {
        REQUIRE_EQ(ls1.data8[i], s1.data8[i]);
    }
}

TEST_CASE(ModRoundTrip_FileIOFidelity) {
    tracker::Song song;
    song.name = "Mod File Test";
    song.num_channels = 4;
    song.order_table = {0, 1};
    song.restart_position = 0;

    song.patterns.emplace_back(64, 4);
    song.patterns[0].get_cell(0, 0).note = 13; // C-1
    song.patterns[0].get_cell(0, 0).instrument = 1;
    song.patterns[0].get_cell(0, 0).effect_type = 0x0F;
    song.patterns[0].get_cell(0, 0).effect_param = 0x06;

    song.patterns.emplace_back(64, 4);
    song.patterns[1].get_cell(32, 1).note = 37; // C-3
    song.patterns[1].get_cell(32, 1).instrument = 1;

    song.instruments.emplace_back();
    auto& inst = song.instruments.back();
    inst.name = "File Sample Inst";
    inst.samples.emplace_back();
    auto& s = inst.samples.back();
    s.name = "File Sample";
    s.volume = 50;
    s.finetune = -1;
    s.loop_type = tracker::LoopType::Forward;
    s.data8 = {10, 20, 30, 40, -10, -20, -30, -40};
    s.length = static_cast<uint32_t>(s.data8.size());
    s.loop_start = 0;
    s.loop_length = 8;

    const std::string tmp_mod = "test_mod_roundtrip_temp.mod";
    auto save_st = tracker::mod::ModWriter::save_to_file(song, tmp_mod);
    REQUIRE(save_st.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_file(tmp_mod);
    std::remove(tmp_mod.c_str());

    REQUIRE(load_res.is_ok());
    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, "Mod File Test");
    REQUIRE_EQ(loaded.num_channels, 4);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 13);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x0F);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x06);
    REQUIRE_EQ(loaded.patterns[1].get_cell(32, 1).note, 37);

    REQUIRE(loaded.instruments.size() >= 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    const auto& ls = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls.name, "File Sample");
    REQUIRE_EQ(ls.volume, 50);
    REQUIRE_EQ(ls.finetune, -1);
    REQUIRE(ls.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(ls.loop_start, 0);
    REQUIRE_EQ(ls.loop_length, 8);
    REQUIRE_EQ(ls.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(ls.data8[i], s.data8[i]);
    }
}

TEST_CASE(ModRoundTrip_MultiChannel8) {
    tracker::Song song;
    song.name = "OctaMOD Song";
    song.num_channels = 8;
    song.order_table = {0, 1, 0};

    // Pattern 0: 64 rows, 8 channels
    song.patterns.emplace_back(64, 8);
    for (uint16_t ch = 0; ch < 8; ++ch) {
        auto& c = song.patterns[0].get_cell(ch * 8, ch);
        c.note = static_cast<uint8_t>(13 + ch * 4); // Notes 13..41
        c.instrument = static_cast<uint8_t>(1 + (ch % 2));
        c.effect_type = static_cast<uint8_t>(ch);
        c.effect_param = static_cast<uint8_t>(ch * 16 + ch);
    }

    // Pattern 1: 64 rows, 8 channels
    song.patterns.emplace_back(64, 8);
    song.patterns[1].get_cell(63, 7).note = 48; // B-3
    song.patterns[1].get_cell(63, 7).instrument = 2;
    song.patterns[1].get_cell(63, 7).effect_type = 0x0B; // Position Jump
    song.patterns[1].get_cell(63, 7).effect_param = 0x00;

    // 2 Instruments
    for (int i = 0; i < 2; ++i) {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Sample " + std::to_string(i + 1);
        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Smp" + std::to_string(i + 1);
        s.volume = static_cast<uint8_t>(40 + i * 20);
        s.finetune = static_cast<int8_t>(i == 0 ? 0 : -3);
        s.loop_type = tracker::LoopType::Forward;
        s.data8 = {10, 20, 30, 40, -10, -20, -30, -40};
        s.length = 8;
        s.loop_start = 2;
        s.loop_length = 4;
    }

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, "OctaMOD Song");
    REQUIRE_EQ(loaded.num_channels, 8);
    REQUIRE_EQ(loaded.patterns.size(), 2);

    for (uint16_t ch = 0; ch < 8; ++ch) {
        const auto& c = loaded.patterns[0].get_cell(ch * 8, ch);
        REQUIRE_EQ(c.note, static_cast<uint8_t>(13 + ch * 4));
        REQUIRE_EQ(c.instrument, static_cast<uint8_t>(1 + (ch % 2)));
        REQUIRE_EQ(c.effect_type, static_cast<uint8_t>(ch));
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(ch * 16 + ch));
    }

    const auto& c_last = loaded.patterns[1].get_cell(63, 7);
    REQUIRE_EQ(c_last.note, 48);
    REQUIRE_EQ(c_last.instrument, 2);
    REQUIRE_EQ(c_last.effect_type, 0x0B);
    REQUIRE_EQ(c_last.effect_param, 0x00);
}

TEST_CASE(ModRoundTrip_AllStandardEffects) {
    tracker::Song song;
    song.name = "All Effects";
    song.num_channels = 4;
    song.order_table = {0};

    song.patterns.emplace_back(64, 4);

    // Row 0..15 on Channel 0: effects 0x0 to 0xF
    for (uint8_t fx = 0x0; fx <= 0xF; ++fx) {
        auto& c = song.patterns[0].get_cell(fx, 0);
        c.note = 25; // C-2
        c.instrument = 1;
        c.effect_type = fx;
        c.effect_param = static_cast<uint8_t>(0x10 + fx);
    }

    // Row 16..31 on Channel 1: E-effects (0x0E with sub-effects 0..F)
    for (uint8_t sub = 0; sub < 16; ++sub) {
        auto& c = song.patterns[0].get_cell(16 + sub, 1);
        c.note = 37; // C-3
        c.instrument = 2;
        c.effect_type = 0x0E;
        c.effect_param = static_cast<uint8_t>((sub << 4) | (sub & 0x0F));
    }

    // 2 Instruments
    song.instruments.emplace_back();
    song.instruments[0].name = "Lead";
    song.instruments[0].samples.emplace_back();
    song.instruments[0].samples[0].name = "Lead Smp";
    song.instruments[0].samples[0].data8 = {10, -10};
    song.instruments[0].samples[0].length = 2;

    song.instruments.emplace_back();
    song.instruments[1].name = "Bass";
    song.instruments[1].samples.emplace_back();
    song.instruments[1].samples[0].name = "Bass Smp";
    song.instruments[1].samples[0].data8 = {20, -20};
    song.instruments[1].samples[0].length = 2;

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    for (uint8_t fx = 0x0; fx <= 0xF; ++fx) {
        const auto& c = loaded.patterns[0].get_cell(fx, 0);
        REQUIRE_EQ(c.note, 25);
        REQUIRE_EQ(c.instrument, 1);
        REQUIRE_EQ(c.effect_type, fx);
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>(0x10 + fx));
    }

    for (uint8_t sub = 0; sub < 16; ++sub) {
        const auto& c = loaded.patterns[0].get_cell(16 + sub, 1);
        REQUIRE_EQ(c.note, 37);
        REQUIRE_EQ(c.instrument, 2);
        REQUIRE_EQ(c.effect_type, 0x0E);
        REQUIRE_EQ(c.effect_param, static_cast<uint8_t>((sub << 4) | (sub & 0x0F)));
    }
}

TEST_CASE(ModRoundTrip_MaxInstrumentsAndLoops) {
    tracker::Song song;
    song.name = "Max 31 Instruments";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    // Create 31 instruments with various volumes, finetunes, and loop configurations
    for (int i = 0; i < 31; ++i) {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Inst " + std::to_string(i + 1);
        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Smp " + std::to_string(i + 1);
        s.volume = static_cast<uint8_t>(i * 2); // 0 to 60
        s.finetune = static_cast<int8_t>((i % 16) - 8); // -8 to +7
        if (i % 2 == 0) {
            s.loop_type = tracker::LoopType::Forward;
            s.loop_start = 2;
            s.loop_length = 4;
        } else {
            s.loop_type = tracker::LoopType::None;
            s.loop_start = 0;
            s.loop_length = 0;
        }
        s.data8 = {1, 2, 3, 4, -1, -2, -3, -4};
        s.length = 8;

        // Place cell in pattern
        if (i < 64) {
            auto& c = song.patterns[0].get_cell(static_cast<uint16_t>(i), 0);
            c.note = static_cast<uint8_t>(13 + (i % 36));
            c.instrument = static_cast<uint8_t>(i + 1);
        }
    }

    auto save_res = tracker::mod::ModWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::mod::ModReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 31);

    for (int i = 0; i < 31; ++i) {
        const auto& orig_s = song.instruments[static_cast<size_t>(i)].samples[0];
        const auto& load_s = loaded.instruments[static_cast<size_t>(i)].samples[0];
        REQUIRE_EQ(load_s.name, orig_s.name);
        REQUIRE_EQ(load_s.volume, orig_s.volume);
        REQUIRE_EQ(load_s.finetune, orig_s.finetune);
        REQUIRE(load_s.loop_type == orig_s.loop_type);
        if (orig_s.loop_type == tracker::LoopType::Forward) {
            REQUIRE_EQ(load_s.loop_start, orig_s.loop_start);
            REQUIRE_EQ(load_s.loop_length, orig_s.loop_length);
        }
        REQUIRE_EQ(load_s.data8.size(), 8);
        for (size_t d = 0; d < 8; ++d) {
            REQUIRE_EQ(load_s.data8[d], orig_s.data8[d]);
        }
    }
}
