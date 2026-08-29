#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(XmRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Chiptune Hero";
    original.tracker_name = "FastTracker v2.00";
    original.num_channels = 4;
    original.default_speed = 6;
    original.default_bpm = 135;
    original.linear_frequency = true;
    original.restart_position = 0;
    original.order_table = {0, 1, 0};

    // Pattern 0: Populated
    original.patterns.emplace_back(64, 4);
    original.patterns[0].get_cell(0, 0).note = 49; // C-4
    original.patterns[0].get_cell(0, 0).instrument = 1;
    original.patterns[0].get_cell(0, 0).volume = 0x40;
    original.patterns[0].get_cell(0, 0).effect_type = 0x0A;
    original.patterns[0].get_cell(0, 0).effect_param = 0x0F;

    original.patterns[0].get_cell(4, 1).note = 97; // Key off

    // Pattern 1: Empty pattern optimization
    original.patterns.emplace_back(64, 4);

    // Instrument 1: With 8-bit sample and envelopes
    original.instruments.emplace_back();
    auto& inst1 = original.instruments.back();
    inst1.name = "Lead Square";
    inst1.volume_fadeout = 256;
    inst1.volume_envelope.enabled = true;
    inst1.volume_envelope.sustain_enabled = true;
    inst1.volume_envelope.sustain_point = 1;
    inst1.volume_envelope.points = {{0, 64}, {10, 48}, {30, 0}};

    inst1.samples.emplace_back();
    auto& s1 = inst1.samples.back();
    s1.name = "Square Wave";
    s1.volume = 60;
    s1.panning = 128;
    s1.finetune = 5;
    s1.relative_note = 0;
    s1.is_16bit = false;
    s1.loop_type = tracker::LoopType::Forward;
    s1.data8 = {0, 32, 64, 32, 0, -32, -64, -32};
    s1.length = static_cast<uint32_t>(s1.data8.size());
    s1.loop_start = 0;
    s1.loop_length = s1.length;

    // Instrument 2: With 16-bit sample
    original.instruments.emplace_back();
    auto& inst2 = original.instruments.back();
    inst2.name = "16bit Pad";
    inst2.samples.emplace_back();
    auto& s2 = inst2.samples.back();
    s2.name = "Pad Wave";
    s2.volume = 55;
    s2.is_16bit = true;
    s2.loop_type = tracker::LoopType::PingPong;
    s2.data16 = {0, 5000, 15000, 30000, 15000, 0, -15000, -30000, -15000};
    s2.length = static_cast<uint32_t>(s2.data16.size());
    s2.loop_start = 2;
    s2.loop_length = 5;

    // Save to XM bytes
    auto save_res = tracker::xm::XmWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& xm_bytes = save_res.value();
    REQUIRE(!xm_bytes.empty());

    // Load back from XM bytes
    auto load_res = tracker::xm::XmReader::load_from_memory(xm_bytes.data(), xm_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();

    // Verify Metadata
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.num_channels, original.num_channels);
    REQUIRE_EQ(loaded.default_speed, original.default_speed);
    REQUIRE_EQ(loaded.default_bpm, original.default_bpm);
    REQUIRE_EQ(loaded.linear_frequency, original.linear_frequency);
    REQUIRE_EQ(loaded.order_table.size(), original.order_table.size());
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.instruments.size(), 2);

    // Verify Cells
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 49);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x0A);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x0F);
    REQUIRE_EQ(loaded.patterns[0].get_cell(4, 1).note, 97);
    REQUIRE(loaded.patterns[1].is_all_empty());

    // Verify 8-bit Sample & Envelope
    REQUIRE_EQ(loaded.instruments[0].name, original.instruments[0].name);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points.size(), 3);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points[1].tick, 10);
    REQUIRE_EQ(loaded.instruments[0].volume_envelope.points[1].value, 48);

    const auto& ls1 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls1.name, s1.name);
    REQUIRE_EQ(ls1.is_16bit, false);
    REQUIRE_EQ(ls1.volume, 60);
    REQUIRE_EQ(ls1.data8.size(), s1.data8.size());
    for (size_t i = 0; i < s1.data8.size(); ++i) {
        REQUIRE_EQ(ls1.data8[i], s1.data8[i]);
    }

    // Verify 16-bit Sample
    REQUIRE_EQ(loaded.instruments[1].name, original.instruments[1].name);
    const auto& ls2 = loaded.instruments[1].samples[0];
    REQUIRE_EQ(ls2.name, s2.name);
    REQUIRE_EQ(ls2.is_16bit, true);
    REQUIRE_EQ(ls2.volume, 55);
    REQUIRE_EQ(ls2.loop_start, 2);
    REQUIRE_EQ(ls2.loop_length, 5);
    REQUIRE_EQ(ls2.data16.size(), s2.data16.size());
    for (size_t i = 0; i < s2.data16.size(); ++i) {
        REQUIRE_EQ(ls2.data16[i], s2.data16[i]);
    }
}

TEST_CASE(XmRoundTrip_ComplexMultiSampleAndKeymaps) {
    tracker::Song song;
    song.name = "Epic Odyssey";
    song.tracker_name = "FastTracker v2.00";
    song.num_channels = 8;
    song.default_speed = 4;
    song.default_bpm = 140;
    song.linear_frequency = true;
    song.restart_position = 1;
    song.order_table = {0, 1, 2, 1, 0};

    // Pattern 0: 64 rows, 8 channels - full variety of cells
    song.patterns.emplace_back(64, 8);
    for (uint16_t row = 0; row < 64; row += 4) {
        for (uint16_t ch = 0; ch < 8; ++ch) {
            auto& cell = song.patterns[0].get_cell(row, ch);
            cell.note = static_cast<uint8_t>(1 + (row + ch) % 96);
            cell.instrument = static_cast<uint8_t>(1 + (ch % 3));
            cell.volume = static_cast<uint8_t>(0x10 + ((row * 2 + ch) % 65));
            cell.effect_type = static_cast<uint8_t>(ch % 16);
            cell.effect_param = static_cast<uint8_t>(row + ch * 16);
        }
    }
    // Set a key-off note and volume column effects
    song.patterns[0].get_cell(63, 7).note = 97; // Key off
    song.patterns[0].get_cell(63, 7).volume = 0x85; // Fine volume down
    song.patterns[0].get_cell(63, 7).effect_type = 0x0C; // Set volume
    song.patterns[0].get_cell(63, 7).effect_param = 0x20;

    // Pattern 1: 32 rows, 8 channels - partially populated
    song.patterns.emplace_back(32, 8);
    song.patterns[1].get_cell(0, 0).note = 25; // C-2
    song.patterns[1].get_cell(0, 0).instrument = 1;
    song.patterns[1].get_cell(16, 2).note = 37; // C-3
    song.patterns[1].get_cell(16, 2).instrument = 2;
    song.patterns[1].get_cell(31, 7).note = 97; // Key off

    // Pattern 2: 128 rows, 8 channels - completely empty
    song.patterns.emplace_back(128, 8);

    // Instrument 1: Multi-sample with 4 samples, envelopes, auto-vibrato
    {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Multi Acoustic";
        inst.volume_fadeout = 1024;
        inst.vibrato_type = 1;  // Square
        inst.vibrato_sweep = 20;
        inst.vibrato_depth = 10;
        inst.vibrato_rate = 15;

        // Sample map: 4 key zones across 96 notes
        for (size_t k = 0; k < 96; ++k) {
            inst.sample_map[k] = static_cast<uint8_t>(k / 24); // 0, 1, 2, 3
        }

        // Volume envelope: 5 points, sustain & loop
        inst.volume_envelope.enabled = true;
        inst.volume_envelope.sustain_enabled = true;
        inst.volume_envelope.loop_enabled = true;
        inst.volume_envelope.sustain_point = 2;
        inst.volume_envelope.loop_start_point = 1;
        inst.volume_envelope.loop_end_point = 3;
        inst.volume_envelope.points = {
            {0, 64}, {10, 50}, {25, 40}, {50, 20}, {100, 0}
        };

        // Panning envelope: 4 points, sustain & loop
        inst.panning_envelope.enabled = true;
        inst.panning_envelope.sustain_enabled = true;
        inst.panning_envelope.loop_enabled = true;
        inst.panning_envelope.sustain_point = 1;
        inst.panning_envelope.loop_start_point = 0;
        inst.panning_envelope.loop_end_point = 2;
        inst.panning_envelope.points = {
            {0, 32}, {15, 64}, {30, 0}, {60, 32}
        };

        // Sample 0: 8-bit Forward Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Zone 1 Low 8b";
            s.volume = 50;
            s.finetune = 3;
            s.panning = 64;
            s.relative_note = -12;
            s.is_16bit = false;
            s.loop_type = tracker::LoopType::Forward;
            s.data8 = {0, 10, 20, 30, 40, 30, 20, 10, 0, -10, -20, -30, -40, -30, -20, -10};
            s.length = static_cast<uint32_t>(s.data8.size());
            s.loop_start = 4;
            s.loop_length = 8;
        }

        // Sample 1: 16-bit PingPong Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Zone 2 Mid 16b";
            s.volume = 60;
            s.finetune = -5;
            s.panning = 192;
            s.relative_note = 12;
            s.is_16bit = true;
            s.loop_type = tracker::LoopType::PingPong;
            s.data16 = {0, 4000, 8000, 12000, 16000, 12000, 8000, 4000, 0, -4000, -8000, -12000, -16000, -12000, -8000, -4000};
            s.length = static_cast<uint32_t>(s.data16.size());
            s.loop_start = 2;
            s.loop_length = 10;
        }

        // Sample 2: 8-bit No Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Zone 3 High 8b";
            s.volume = 64;
            s.finetune = 0;
            s.panning = 128;
            s.relative_note = 0;
            s.is_16bit = false;
            s.loop_type = tracker::LoopType::None;
            s.data8 = {5, 15, 25, 35, 45, 55, 45, 35, 25, 15, 5, -5, -15, -25};
            s.length = static_cast<uint32_t>(s.data8.size());
            s.loop_start = 0;
            s.loop_length = 0;
        }

        // Sample 3: 16-bit Forward Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Zone 4 Top 16b";
            s.volume = 40;
            s.finetune = 10;
            s.panning = 255;
            s.relative_note = -24;
            s.is_16bit = true;
            s.loop_type = tracker::LoopType::Forward;
            s.data16 = {100, 500, 1500, 4500, 9000, 15000, 9000, 4500, 1500, 500, 100, -500, -1500, -4500};
            s.length = static_cast<uint32_t>(s.data16.size());
            s.loop_start = 1;
            s.loop_length = 8;
        }
    }

    // Instrument 2: Empty instrument (no samples)
    {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Empty Synth";
        inst.volume_fadeout = 0;
    }

    // Instrument 3: Single 16-bit sample
    {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Solo Sine 16b";
        inst.volume_fadeout = 512;
        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Sine Wave";
        s.volume = 62;
        s.finetune = 0;
        s.panning = 128;
        s.relative_note = 0;
        s.is_16bit = true;
        s.loop_type = tracker::LoopType::PingPong;
        s.data16 = {0, 7071, 10000, 7071, 0, -7071, -10000, -7071};
        s.length = static_cast<uint32_t>(s.data16.size());
        s.loop_start = 0;
        s.loop_length = s.length;
    }

    // Write and read back
    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();

    // Verify Song Header
    REQUIRE_EQ(loaded.name, song.name);
    REQUIRE_EQ(loaded.tracker_name, song.tracker_name);
    REQUIRE_EQ(loaded.num_channels, song.num_channels);
    REQUIRE_EQ(loaded.default_speed, song.default_speed);
    REQUIRE_EQ(loaded.default_bpm, song.default_bpm);
    REQUIRE_EQ(loaded.linear_frequency, song.linear_frequency);
    REQUIRE_EQ(loaded.restart_position, song.restart_position);
    REQUIRE_EQ(loaded.order_table.size(), song.order_table.size());
    for (size_t i = 0; i < song.order_table.size(); ++i) {
        REQUIRE_EQ(loaded.order_table[i], song.order_table[i]);
    }

    // Verify Patterns
    REQUIRE_EQ(loaded.patterns.size(), 3);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[0].num_channels, 8);
    for (uint16_t r = 0; r < 64; ++r) {
        for (uint16_t c = 0; c < 8; ++c) {
            const auto& orig_cell = song.patterns[0].get_cell(r, c);
            const auto& load_cell = loaded.patterns[0].get_cell(r, c);
            REQUIRE_EQ(load_cell.note, orig_cell.note);
            REQUIRE_EQ(load_cell.instrument, orig_cell.instrument);
            REQUIRE_EQ(load_cell.volume, orig_cell.volume);
            REQUIRE_EQ(load_cell.effect_type, orig_cell.effect_type);
            REQUIRE_EQ(load_cell.effect_param, orig_cell.effect_param);
        }
    }

    REQUIRE_EQ(loaded.patterns[1].num_rows, 32);
    REQUIRE_EQ(loaded.patterns[1].num_channels, 8);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).note, 25);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 2).note, 37);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 2).instrument, 2);
    REQUIRE_EQ(loaded.patterns[1].get_cell(31, 7).note, 97);

    REQUIRE_EQ(loaded.patterns[2].num_rows, 128);
    REQUIRE_EQ(loaded.patterns[2].num_channels, 8);
    REQUIRE(loaded.patterns[2].is_all_empty());

    // Verify Instruments
    REQUIRE_EQ(loaded.instruments.size(), 3);

    // Instrument 1: Multi-sample
    {
        const auto& l_inst = loaded.instruments[0];
        const auto& o_inst = song.instruments[0];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.volume_fadeout, o_inst.volume_fadeout);
        REQUIRE_EQ(l_inst.vibrato_type, o_inst.vibrato_type);
        REQUIRE_EQ(l_inst.vibrato_sweep, o_inst.vibrato_sweep);
        REQUIRE_EQ(l_inst.vibrato_depth, o_inst.vibrato_depth);
        REQUIRE_EQ(l_inst.vibrato_rate, o_inst.vibrato_rate);

        // Keymaps
        for (size_t k = 0; k < 96; ++k) {
            REQUIRE_EQ(l_inst.sample_map[k], o_inst.sample_map[k]);
        }

        // Volume Envelope
        REQUIRE_EQ(l_inst.volume_envelope.enabled, o_inst.volume_envelope.enabled);
        REQUIRE_EQ(l_inst.volume_envelope.sustain_enabled, o_inst.volume_envelope.sustain_enabled);
        REQUIRE_EQ(l_inst.volume_envelope.loop_enabled, o_inst.volume_envelope.loop_enabled);
        REQUIRE_EQ(l_inst.volume_envelope.sustain_point, o_inst.volume_envelope.sustain_point);
        REQUIRE_EQ(l_inst.volume_envelope.loop_start_point, o_inst.volume_envelope.loop_start_point);
        REQUIRE_EQ(l_inst.volume_envelope.loop_end_point, o_inst.volume_envelope.loop_end_point);
        REQUIRE_EQ(l_inst.volume_envelope.points.size(), o_inst.volume_envelope.points.size());
        for (size_t p = 0; p < o_inst.volume_envelope.points.size(); ++p) {
            REQUIRE_EQ(l_inst.volume_envelope.points[p].tick, o_inst.volume_envelope.points[p].tick);
            REQUIRE_EQ(l_inst.volume_envelope.points[p].value, o_inst.volume_envelope.points[p].value);
        }

        // Panning Envelope
        REQUIRE_EQ(l_inst.panning_envelope.enabled, o_inst.panning_envelope.enabled);
        REQUIRE_EQ(l_inst.panning_envelope.sustain_enabled, o_inst.panning_envelope.sustain_enabled);
        REQUIRE_EQ(l_inst.panning_envelope.loop_enabled, o_inst.panning_envelope.loop_enabled);
        REQUIRE_EQ(l_inst.panning_envelope.sustain_point, o_inst.panning_envelope.sustain_point);
        REQUIRE_EQ(l_inst.panning_envelope.loop_start_point, o_inst.panning_envelope.loop_start_point);
        REQUIRE_EQ(l_inst.panning_envelope.loop_end_point, o_inst.panning_envelope.loop_end_point);
        REQUIRE_EQ(l_inst.panning_envelope.points.size(), o_inst.panning_envelope.points.size());
        for (size_t p = 0; p < o_inst.panning_envelope.points.size(); ++p) {
            REQUIRE_EQ(l_inst.panning_envelope.points[p].tick, o_inst.panning_envelope.points[p].tick);
            REQUIRE_EQ(l_inst.panning_envelope.points[p].value, o_inst.panning_envelope.points[p].value);
        }

        // 4 Samples
        REQUIRE_EQ(l_inst.samples.size(), 4);
        for (size_t s = 0; s < 4; ++s) {
            const auto& ls = l_inst.samples[s];
            const auto& os = o_inst.samples[s];
            REQUIRE_EQ(ls.name, os.name);
            REQUIRE_EQ(ls.volume, os.volume);
            REQUIRE_EQ(ls.finetune, os.finetune);
            REQUIRE_EQ(ls.panning, os.panning);
            REQUIRE_EQ(ls.relative_note, os.relative_note);
            REQUIRE_EQ(ls.is_16bit, os.is_16bit);
            REQUIRE_EQ(static_cast<uint8_t>(ls.loop_type), static_cast<uint8_t>(os.loop_type));
            REQUIRE_EQ(ls.loop_start, os.loop_start);
            REQUIRE_EQ(ls.loop_length, os.loop_length);

            if (os.is_16bit) {
                REQUIRE_EQ(ls.data16.size(), os.data16.size());
                for (size_t d = 0; d < os.data16.size(); ++d) {
                    REQUIRE_EQ(ls.data16[d], os.data16[d]);
                }
            } else {
                REQUIRE_EQ(ls.data8.size(), os.data8.size());
                for (size_t d = 0; d < os.data8.size(); ++d) {
                    REQUIRE_EQ(ls.data8[d], os.data8[d]);
                }
            }
        }
    }

    // Instrument 2: Empty
    {
        const auto& l_inst = loaded.instruments[1];
        const auto& o_inst = song.instruments[1];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.samples.size(), 0);
    }

    // Instrument 3: Single 16-bit
    {
        const auto& l_inst = loaded.instruments[2];
        const auto& o_inst = song.instruments[2];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.samples.size(), 1);
        const auto& ls = l_inst.samples[0];
        const auto& os = o_inst.samples[0];
        REQUIRE_EQ(ls.name, os.name);
        REQUIRE_EQ(ls.is_16bit, true);
        REQUIRE_EQ(ls.volume, os.volume);
        REQUIRE_EQ(ls.loop_start, os.loop_start);
        REQUIRE_EQ(ls.loop_length, os.loop_length);
        REQUIRE_EQ(ls.data16.size(), os.data16.size());
        for (size_t d = 0; d < os.data16.size(); ++d) {
            REQUIRE_EQ(ls.data16[d], os.data16[d]);
        }
    }
}

TEST_CASE(XmRoundTrip_FileIOFidelity) {
    tracker::Song song;
    song.name = "File IO Test Song";
    song.tracker_name = "FastTracker v2.00";
    song.num_channels = 6;
    song.default_speed = 3;
    song.default_bpm = 150;
    song.linear_frequency = true;
    song.restart_position = 0;
    song.order_table = {0, 1};

    song.patterns.emplace_back(32, 6);
    song.patterns[0].get_cell(0, 0).note = 60;
    song.patterns[0].get_cell(0, 0).instrument = 1;
    song.patterns[0].get_cell(0, 0).volume = 0x30;
    song.patterns[0].get_cell(0, 0).effect_type = 0x01; // Portamento up
    song.patterns[0].get_cell(0, 0).effect_param = 0x05;

    song.patterns.emplace_back(32, 6);
    song.patterns[1].get_cell(10, 3).note = 97; // Key off

    song.instruments.emplace_back();
    auto& inst = song.instruments.back();
    inst.name = "Lead File";
    inst.volume_fadeout = 300;
    inst.samples.emplace_back();
    auto& s = inst.samples.back();
    s.name = "Sawtooth";
    s.volume = 58;
    s.is_16bit = false;
    s.loop_type = tracker::LoopType::Forward;
    s.data8 = {-64, -48, -32, -16, 0, 16, 32, 48, 64, 48, 32, 16, 0, -16, -32, -48};
    s.length = static_cast<uint32_t>(s.data8.size());
    s.loop_start = 0;
    s.loop_length = s.length;

    const std::string tmp_filename = "/tmp/test_xm_roundtrip_file.xm";
    auto save_status = tracker::xm::XmWriter::save_to_file(song, tmp_filename);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_file(tmp_filename);
    std::remove(tmp_filename.c_str());

    REQUIRE(load_res.is_ok());
    const auto& loaded = load_res.value();

    REQUIRE_EQ(loaded.name, song.name);
    REQUIRE_EQ(loaded.num_channels, 6);
    REQUIRE_EQ(loaded.default_speed, 3);
    REQUIRE_EQ(loaded.default_bpm, 150);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 60);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x01);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x05);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 3).note, 97);
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].name, "Lead File");
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples[0].data8.size(), s.data8.size());
    for (size_t i = 0; i < s.data8.size(); ++i) {
        REQUIRE_EQ(loaded.instruments[0].samples[0].data8[i], s.data8[i]);
    }
}

TEST_CASE(XmRoundTrip_AmigaFrequencyAndMaxChannels) {
    tracker::Song song;
    song.name = "Amiga 32Ch Max";
    song.tracker_name = "FastTracker v2.00";
    song.num_channels = 32;
    song.default_speed = 6;
    song.default_bpm = 125;
    song.linear_frequency = false; // Amiga frequency table
    song.restart_position = 0;
    song.order_table = {0};

    song.patterns.emplace_back(64, 32);
    song.patterns[0].get_cell(0, 31).note = 48; // C-4 on channel 32
    song.patterns[0].get_cell(0, 31).instrument = 1;

    song.instruments.emplace_back();
    song.instruments[0].name = "Amiga Bass";
    song.instruments[0].samples.emplace_back();
    song.instruments[0].samples[0].name = "Bass";
    song.instruments[0].samples[0].volume = 64;
    song.instruments[0].samples[0].is_16bit = false;
    song.instruments[0].samples[0].data8 = {10, 20, 30, -10, -20, -30};
    song.instruments[0].samples[0].length = 6;

    auto save_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, song.name);
    REQUIRE_EQ(loaded.num_channels, 32);
    REQUIRE_EQ(loaded.linear_frequency, false);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 31).note, 48);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 31).instrument, 1);
    REQUIRE_EQ(loaded.instruments[0].name, "Amiga Bass");
    REQUIRE_EQ(loaded.instruments[0].samples[0].data8.size(), 6);
}
