#include "test_main.hpp"
#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_reader.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(ItRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Impulse Symphony";
    original.tracker_name = "Impulse Tracker";
    original.message = "Impulse Tracker Song Message\r\nLine 2: 64 channels, 120 keymaps, 25 nodes!\r\nCreated with audio_tracker.";
    original.num_channels = 64;
    original.default_speed = 6;
    original.default_bpm = 125;
    original.global_volume = 120;
    original.mix_volume = 45;
    original.pan_separation = 128;
    original.linear_frequency = true;
    original.restart_position = 0;
    original.order_table = {0, 1, 2, 0, 255};

    for (size_t c = 0; c < 64; ++c) {
        original.channel_panning[c] = static_cast<uint8_t>((c * 17) % 65);
        original.channel_volume[c] = static_cast<uint8_t>(64 - (c % 33));
    }

    // Pattern 0: 64 rows, 64 channels - full diversity of cells across all channels
    original.patterns.emplace_back(64, 64);
    for (uint16_t row = 0; row < 64; ++row) {
        for (uint16_t ch = 0; ch < 64; ++ch) {
            if ((row + ch) % 5 == 0) {
                auto& cell = original.patterns[0].get_cell(row, ch);
                cell.note = static_cast<uint8_t>(1 + (row * 3 + ch) % 119);
                cell.instrument = static_cast<uint8_t>(1 + (ch % 2));
                cell.volume = static_cast<uint8_t>(0x10 + ((row + ch) % 65));
                cell.effect_type = static_cast<uint8_t>(1 + (ch % 26)); // Commands 1..26 (A..Z)
                cell.effect_param = static_cast<uint8_t>((row * 4 + ch) & 0xFF);
            }
        }
    }

    // Special notes on specific cells
    original.patterns[0].get_cell(63, 0).note = tracker::it::IT_NOTE_CUT;  // 253
    original.patterns[0].get_cell(63, 1).note = tracker::it::IT_NOTE_OFF;  // 254
    original.patterns[0].get_cell(63, 2).note = tracker::it::IT_NOTE_FADE; // 255

    // Pattern 1: 32 rows, 64 channels - partially populated
    original.patterns.emplace_back(32, 64);
    original.patterns[1].get_cell(0, 0).note = 49; // C-4
    original.patterns[1].get_cell(0, 0).instrument = 1;
    original.patterns[1].get_cell(16, 10).note = 61; // C-5
    original.patterns[1].get_cell(16, 10).instrument = 2;
    original.patterns[1].get_cell(31, 63).note = tracker::it::IT_NOTE_CUT;

    // Pattern 2: 128 rows, 64 channels - completely empty
    original.patterns.emplace_back(128, 64);

    // Instrument 1: Complex Multi-Sample with envelopes, NNAs, 120 keymaps
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Stereo Lead";
        inst.filename = "LEAD.ITI";
        inst.nna = tracker::NewNoteAction::NoteFade;
        inst.dct = tracker::DuplicateCheckType::Sample;
        inst.dca = tracker::DuplicateCheckAction::NoteFade;
        inst.volume_fadeout = 768;
        inst.global_volume = 115;
        inst.default_panning = 160;

        // 120-note keyboard mapping: 3 zones
        for (size_t k = 0; k < 120; ++k) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k);
            if (k < 40) {
                inst.keyboard_map[k].sample = 1;
            } else if (k < 80) {
                inst.keyboard_map[k].sample = 2;
            } else {
                inst.keyboard_map[k].sample = 3;
            }
        }

        // Volume Envelope: 4 points, loop and sustain
        inst.volume_envelope.enabled = true;
        inst.volume_envelope.sustain_enabled = true;
        inst.volume_envelope.loop_enabled = true;
        inst.volume_envelope.sustain_point = 1;
        inst.volume_envelope.loop_start_point = 0;
        inst.volume_envelope.loop_end_point = 2;
        inst.volume_envelope.points = {
            {0, 64}, {15, 48}, {30, 20}, {60, 0}
        };

        // Panning Envelope: 3 points, loop and sustain
        inst.panning_envelope.enabled = true;
        inst.panning_envelope.sustain_enabled = true;
        inst.panning_envelope.loop_enabled = true;
        inst.panning_envelope.sustain_point = 1;
        inst.panning_envelope.loop_start_point = 0;
        inst.panning_envelope.loop_end_point = 2;
        inst.panning_envelope.points = {
            {0, 32}, {20, 64}, {40, 0}
        };

        // Pitch Envelope: 2 points
        inst.pitch_envelope.enabled = true;
        inst.pitch_envelope.sustain_enabled = true;
        inst.pitch_envelope.loop_enabled = true;
        inst.pitch_envelope.sustain_point = 0;
        inst.pitch_envelope.loop_start_point = 0;
        inst.pitch_envelope.loop_end_point = 1;
        inst.pitch_envelope.points = {
            {0, 32}, {25, 48}
        };

        // Sample 1: 8-bit Forward Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Lead Smp 1 (8b)";
            s.volume = 60;
            s.global_volume = 64;
            s.panning = 64;
            s.c5_speed = 16726;
            s.is_16bit = false;
            s.loop_type = tracker::LoopType::Forward;
            s.data8 = {10, 20, 30, 40, 30, 20, 10, 0, -10, -20, -30, -40, -30, -20, -10, 0};
            s.length = static_cast<uint32_t>(s.data8.size());
            s.loop_start = 4;
            s.loop_length = 8;
            s.vibrato_sweep = 10;
            s.vibrato_depth = 15;
            s.vibrato_rate = 20;
            s.vibrato_type = 1;
        }

        // Sample 2: 16-bit Sustain Loop (PingPong)
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Lead Smp 2 (16b)";
            s.volume = 55;
            s.global_volume = 60;
            s.panning = 192;
            s.c5_speed = 44100;
            s.is_16bit = true;
            s.sustain_loop_type = tracker::LoopType::PingPong;
            s.data16 = {500, 1500, 3000, 5000, 3000, 1500, -500, -1500, -3000, -5000, -3000, -1500};
            s.length = static_cast<uint32_t>(s.data16.size());
            s.sustain_loop_start = 2;
            s.sustain_loop_length = 6;
        }

        // Sample 3: 8-bit No Loop
        {
            inst.samples.emplace_back();
            auto& s = inst.samples.back();
            s.name = "Lead Smp 3 (8b)";
            s.volume = 64;
            s.global_volume = 64;
            s.panning = 128;
            s.c5_speed = 8363;
            s.is_16bit = false;
            s.loop_type = tracker::LoopType::None;
            s.data8 = {50, 40, 30, 20, 10, 0, -10, -20};
            s.length = static_cast<uint32_t>(s.data8.size());
        }
    }

    // Instrument 2: Empty instrument
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Empty Instrument";
        inst.volume_fadeout = 0;
    }

    // Instrument 3: Single 16-bit sample with Forward Loop
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Acoustic Grand";
        inst.filename = "PIANO.ITI";
        inst.nna = tracker::NewNoteAction::NoteOff;
        inst.dct = tracker::DuplicateCheckType::Note;
        inst.dca = tracker::DuplicateCheckAction::NoteOff;
        inst.volume_fadeout = 512;
        inst.global_volume = 128;

        for (size_t k = 0; k < 120; ++k) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k);
            inst.keyboard_map[k].sample = 1;
        }

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Piano Sample";
        s.volume = 64;
        s.global_volume = 64;
        s.c5_speed = 22050;
        s.is_16bit = true;
        s.loop_type = tracker::LoopType::Forward;
        s.data16 = {1000, 2000, 3000, 4000, 3000, 2000, 1000, 0, -1000, -2000, -3000, -4000, -3000, -2000, -1000, 0};
        s.length = static_cast<uint32_t>(s.data16.size());
        s.loop_start = 4;
        s.loop_length = 8;
    }

    // Save to IT bytes
    auto save_res = tracker::it::ItWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& it_bytes = save_res.value();
    REQUIRE(!it_bytes.empty());

    // Load back from IT bytes
    auto load_res = tracker::it::ItReader::load_from_memory(it_bytes.data(), it_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();

    // Verify Metadata
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.message, original.message);
    REQUIRE_EQ(loaded.num_channels, original.num_channels);
    REQUIRE_EQ(loaded.default_speed, original.default_speed);
    REQUIRE_EQ(loaded.default_bpm, original.default_bpm);
    REQUIRE_EQ(loaded.global_volume, original.global_volume);
    REQUIRE_EQ(loaded.mix_volume, original.mix_volume);
    REQUIRE_EQ(loaded.pan_separation, original.pan_separation);
    REQUIRE_EQ(loaded.linear_frequency, original.linear_frequency);
    REQUIRE_EQ(loaded.order_table.size(), original.order_table.size());
    for (size_t i = 0; i < original.order_table.size(); ++i) {
        REQUIRE_EQ(loaded.order_table[i], original.order_table[i]);
    }

    // Verify Channel Pan and Volume
    for (size_t c = 0; c < 64; ++c) {
        REQUIRE_EQ(loaded.channel_panning[c], original.channel_panning[c]);
        REQUIRE_EQ(loaded.channel_volume[c], original.channel_volume[c]);
    }

    // Verify Patterns
    REQUIRE_EQ(loaded.patterns.size(), 3);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[0].num_channels, 64);
    for (uint16_t r = 0; r < 64; ++r) {
        for (uint16_t c = 0; c < 64; ++c) {
            const auto& orig_cell = original.patterns[0].get_cell(r, c);
            const auto& load_cell = loaded.patterns[0].get_cell(r, c);
            REQUIRE_EQ(load_cell.note, orig_cell.note);
            REQUIRE_EQ(load_cell.instrument, orig_cell.instrument);
            REQUIRE_EQ(load_cell.volume, orig_cell.volume);
            REQUIRE_EQ(load_cell.effect_type, orig_cell.effect_type);
            REQUIRE_EQ(load_cell.effect_param, orig_cell.effect_param);
        }
    }

    REQUIRE_EQ(loaded.patterns[0].get_cell(63, 0).note, tracker::it::IT_NOTE_CUT);
    REQUIRE_EQ(loaded.patterns[0].get_cell(63, 1).note, tracker::it::IT_NOTE_OFF);
    REQUIRE_EQ(loaded.patterns[0].get_cell(63, 2).note, tracker::it::IT_NOTE_FADE);

    REQUIRE_EQ(loaded.patterns[1].num_rows, 32);
    REQUIRE_EQ(loaded.patterns[1].num_channels, 64);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).note, 49);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 10).note, 61);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 10).instrument, 2);
    REQUIRE_EQ(loaded.patterns[1].get_cell(31, 63).note, tracker::it::IT_NOTE_CUT);

    REQUIRE_EQ(loaded.patterns[2].num_rows, 128);
    REQUIRE_EQ(loaded.patterns[2].num_channels, 64);
    REQUIRE(loaded.patterns[2].is_all_empty());

    // Verify Instruments
    REQUIRE_EQ(loaded.instruments.size(), 3);

    // Instrument 1: Multi-sample with envelopes
    {
        const auto& l_inst = loaded.instruments[0];
        const auto& o_inst = original.instruments[0];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE(l_inst.nna == o_inst.nna);
        REQUIRE(l_inst.dct == o_inst.dct);
        REQUIRE(l_inst.dca == o_inst.dca);
        REQUIRE_EQ(l_inst.volume_fadeout, o_inst.volume_fadeout);
        REQUIRE_EQ(l_inst.global_volume, o_inst.global_volume);
        REQUIRE_EQ(l_inst.default_panning, o_inst.default_panning);

        // Keymaps
        for (size_t k = 0; k < 120; ++k) {
            REQUIRE_EQ(l_inst.keyboard_map[k].note, o_inst.keyboard_map[k].note);
            REQUIRE_EQ(l_inst.keyboard_map[k].sample, o_inst.keyboard_map[k].sample);
        }

        // Volume Envelope
        REQUIRE_EQ(l_inst.volume_envelope.enabled, o_inst.volume_envelope.enabled);
        REQUIRE_EQ(l_inst.volume_envelope.sustain_enabled, o_inst.volume_envelope.sustain_enabled);
        REQUIRE_EQ(l_inst.volume_envelope.loop_enabled, o_inst.volume_envelope.loop_enabled);
        REQUIRE_EQ(l_inst.volume_envelope.points.size(), o_inst.volume_envelope.points.size());
        for (size_t p = 0; p < o_inst.volume_envelope.points.size(); ++p) {
            REQUIRE_EQ(l_inst.volume_envelope.points[p].tick, o_inst.volume_envelope.points[p].tick);
            REQUIRE_EQ(l_inst.volume_envelope.points[p].value, o_inst.volume_envelope.points[p].value);
        }

        // Panning Envelope
        REQUIRE_EQ(l_inst.panning_envelope.enabled, o_inst.panning_envelope.enabled);
        REQUIRE_EQ(l_inst.panning_envelope.sustain_enabled, o_inst.panning_envelope.sustain_enabled);
        REQUIRE_EQ(l_inst.panning_envelope.loop_enabled, o_inst.panning_envelope.loop_enabled);
        REQUIRE_EQ(l_inst.panning_envelope.points.size(), o_inst.panning_envelope.points.size());
        for (size_t p = 0; p < o_inst.panning_envelope.points.size(); ++p) {
            REQUIRE_EQ(l_inst.panning_envelope.points[p].tick, o_inst.panning_envelope.points[p].tick);
            REQUIRE_EQ(l_inst.panning_envelope.points[p].value, o_inst.panning_envelope.points[p].value);
        }

        // Pitch Envelope
        REQUIRE_EQ(l_inst.pitch_envelope.enabled, o_inst.pitch_envelope.enabled);
        REQUIRE_EQ(l_inst.pitch_envelope.sustain_enabled, o_inst.pitch_envelope.sustain_enabled);
        REQUIRE_EQ(l_inst.pitch_envelope.loop_enabled, o_inst.pitch_envelope.loop_enabled);
        REQUIRE_EQ(l_inst.pitch_envelope.points.size(), o_inst.pitch_envelope.points.size());
        for (size_t p = 0; p < o_inst.pitch_envelope.points.size(); ++p) {
            REQUIRE_EQ(l_inst.pitch_envelope.points[p].tick, o_inst.pitch_envelope.points[p].tick);
            REQUIRE_EQ(l_inst.pitch_envelope.points[p].value, o_inst.pitch_envelope.points[p].value);
        }

        // 3 Samples
        REQUIRE_EQ(l_inst.samples.size(), 3);

        // Sample 1: 8-bit Forward Loop
        {
            const auto& ls1 = l_inst.samples[0];
            const auto& os1 = o_inst.samples[0];
            REQUIRE_EQ(ls1.name, os1.name);
            REQUIRE_EQ(ls1.volume, os1.volume);
            REQUIRE_EQ(ls1.global_volume, os1.global_volume);
            REQUIRE_EQ(ls1.panning, os1.panning);
            REQUIRE_EQ(ls1.c5_speed, os1.c5_speed);
            REQUIRE_EQ(ls1.is_16bit, false);
            REQUIRE(ls1.loop_type == tracker::LoopType::Forward);
            REQUIRE_EQ(ls1.loop_start, os1.loop_start);
            REQUIRE_EQ(ls1.loop_length, os1.loop_length);
            REQUIRE_EQ(ls1.vibrato_sweep, os1.vibrato_sweep);
            REQUIRE_EQ(ls1.vibrato_depth, os1.vibrato_depth);
            REQUIRE_EQ(ls1.vibrato_rate, os1.vibrato_rate);
            REQUIRE_EQ(ls1.vibrato_type, os1.vibrato_type);
            REQUIRE_EQ(ls1.data8.size(), os1.data8.size());
            for (size_t d = 0; d < os1.data8.size(); ++d) {
                REQUIRE_EQ(ls1.data8[d], os1.data8[d]);
            }
        }

        // Sample 2: 16-bit Sustain Loop PingPong
        {
            const auto& ls2 = l_inst.samples[1];
            const auto& os2 = o_inst.samples[1];
            REQUIRE_EQ(ls2.name, os2.name);
            REQUIRE_EQ(ls2.volume, os2.volume);
            REQUIRE_EQ(ls2.global_volume, os2.global_volume);
            REQUIRE_EQ(ls2.panning, os2.panning);
            REQUIRE_EQ(ls2.c5_speed, os2.c5_speed);
            REQUIRE_EQ(ls2.is_16bit, true);
            REQUIRE(ls2.sustain_loop_type == tracker::LoopType::PingPong);
            REQUIRE_EQ(ls2.sustain_loop_start, os2.sustain_loop_start);
            REQUIRE_EQ(ls2.sustain_loop_length, os2.sustain_loop_length);
            REQUIRE_EQ(ls2.data16.size(), os2.data16.size());
            for (size_t d = 0; d < os2.data16.size(); ++d) {
                REQUIRE_EQ(ls2.data16[d], os2.data16[d]);
            }
        }

        // Sample 3: 8-bit No Loop
        {
            const auto& ls3 = l_inst.samples[2];
            const auto& os3 = o_inst.samples[2];
            REQUIRE_EQ(ls3.name, os3.name);
            REQUIRE_EQ(ls3.volume, os3.volume);
            REQUIRE_EQ(ls3.c5_speed, os3.c5_speed);
            REQUIRE_EQ(ls3.is_16bit, false);
            REQUIRE(ls3.loop_type == tracker::LoopType::None);
            REQUIRE_EQ(ls3.data8.size(), os3.data8.size());
            for (size_t d = 0; d < os3.data8.size(); ++d) {
                REQUIRE_EQ(ls3.data8[d], os3.data8[d]);
            }
        }
    }

    // Instrument 2: Empty
    {
        const auto& l_inst = loaded.instruments[1];
        const auto& o_inst = original.instruments[1];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.samples.size(), 0);
    }

    // Instrument 3: 16-bit Piano Sample
    {
        const auto& l_inst = loaded.instruments[2];
        const auto& o_inst = original.instruments[2];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE(l_inst.nna == tracker::NewNoteAction::NoteOff);
        REQUIRE(l_inst.dct == tracker::DuplicateCheckType::Note);
        REQUIRE(l_inst.dca == tracker::DuplicateCheckAction::NoteOff);
        REQUIRE_EQ(l_inst.volume_fadeout, 512);
        REQUIRE_EQ(l_inst.global_volume, 128);

        REQUIRE_EQ(l_inst.samples.size(), 1);
        const auto& ls = l_inst.samples[0];
        const auto& os = o_inst.samples[0];
        REQUIRE_EQ(ls.name, os.name);
        REQUIRE_EQ(ls.is_16bit, true);
        REQUIRE_EQ(ls.volume, 64);
        REQUIRE_EQ(ls.c5_speed, 22050);
        REQUIRE(ls.loop_type == tracker::LoopType::Forward);
        REQUIRE_EQ(ls.loop_start, 4);
        REQUIRE_EQ(ls.loop_length, 8);
        REQUIRE_EQ(ls.data16.size(), os.data16.size());
        for (size_t d = 0; d < os.data16.size(); ++d) {
            REQUIRE_EQ(ls.data16[d], os.data16[d]);
        }
    }
}

TEST_CASE(ItRoundTrip_25NodeEnvelopes) {
    tracker::Song song;
    song.name = "Max 25 Nodes Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "25 Node Synth";

    // Build 25 points for Volume Envelope
    inst.volume_envelope.enabled = true;
    inst.volume_envelope.loop_enabled = true;
    inst.volume_envelope.sustain_enabled = true;
    inst.volume_envelope.loop_start_point = 5;
    inst.volume_envelope.loop_end_point = 20;
    inst.volume_envelope.sustain_point = 12;
    for (uint16_t i = 0; i < 25; ++i) {
        inst.volume_envelope.points.push_back({
            static_cast<uint16_t>(i * 10),
            static_cast<uint16_t>((i % 2 == 0) ? 64 : 10)
        });
    }

    // Build 25 points for Panning Envelope
    inst.panning_envelope.enabled = true;
    inst.panning_envelope.loop_enabled = true;
    inst.panning_envelope.sustain_enabled = true;
    inst.panning_envelope.loop_start_point = 2;
    inst.panning_envelope.loop_end_point = 18;
    inst.panning_envelope.sustain_point = 8;
    for (uint16_t i = 0; i < 25; ++i) {
        inst.panning_envelope.points.push_back({
            static_cast<uint16_t>(i * 8),
            static_cast<uint16_t>(i * 2)
        });
    }

    // Build 25 points for Pitch Envelope
    inst.pitch_envelope.enabled = true;
    inst.pitch_envelope.loop_enabled = true;
    inst.pitch_envelope.sustain_enabled = true;
    inst.pitch_envelope.loop_start_point = 1;
    inst.pitch_envelope.loop_end_point = 22;
    inst.pitch_envelope.sustain_point = 15;
    for (uint16_t i = 0; i < 25; ++i) {
        inst.pitch_envelope.points.push_back({
            static_cast<uint16_t>(i * 12),
            static_cast<uint16_t>(32 + ((i % 2 == 0) ? (i / 2) : -(i / 2)))
        });
    }

    tracker::Sample smp;
    smp.name = "Sine";
    smp.data8 = {0, 64, 0, -64};
    smp.length = 4;
    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto save_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& li = loaded.instruments[0];
    const auto& oi = song.instruments[0];

    // Check Volume Envelope
    REQUIRE(li.volume_envelope.enabled);
    REQUIRE(li.volume_envelope.loop_enabled);
    REQUIRE(li.volume_envelope.sustain_enabled);
    REQUIRE_EQ(li.volume_envelope.points.size(), 25);
    for (size_t p = 0; p < 25; ++p) {
        REQUIRE_EQ(li.volume_envelope.points[p].tick, oi.volume_envelope.points[p].tick);
        REQUIRE_EQ(li.volume_envelope.points[p].value, oi.volume_envelope.points[p].value);
    }

    // Check Panning Envelope
    REQUIRE(li.panning_envelope.enabled);
    REQUIRE(li.panning_envelope.loop_enabled);
    REQUIRE(li.panning_envelope.sustain_enabled);
    REQUIRE_EQ(li.panning_envelope.points.size(), 25);
    for (size_t p = 0; p < 25; ++p) {
        REQUIRE_EQ(li.panning_envelope.points[p].tick, oi.panning_envelope.points[p].tick);
        REQUIRE_EQ(li.panning_envelope.points[p].value, oi.panning_envelope.points[p].value);
    }

    // Check Pitch Envelope
    REQUIRE(li.pitch_envelope.enabled);
    REQUIRE(li.pitch_envelope.loop_enabled);
    REQUIRE(li.pitch_envelope.sustain_enabled);
    REQUIRE_EQ(li.pitch_envelope.points.size(), 25);
    for (size_t p = 0; p < 25; ++p) {
        REQUIRE_EQ(li.pitch_envelope.points[p].tick, oi.pitch_envelope.points[p].tick);
        REQUIRE_EQ(li.pitch_envelope.points[p].value, oi.pitch_envelope.points[p].value);
    }
}

TEST_CASE(ItRoundTrip_120NoteKeyboardMap) {
    tracker::Song song;
    song.name = "Keymap 120 Notes";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Drum Multi-Map";

    // 4 sample zones across 120 notes:
    // 0..29 -> note = k, sample = 1
    // 30..59 -> note = k + 12, sample = 2
    // 60..89 -> note = k - 12, sample = 3
    // 90..119 -> note = 60, sample = 4
    for (size_t k = 0; k < 120; ++k) {
        if (k < 30) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k);
            inst.keyboard_map[k].sample = 1;
        } else if (k < 60) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k + 12);
            inst.keyboard_map[k].sample = 2;
        } else if (k < 90) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k - 12);
            inst.keyboard_map[k].sample = 3;
        } else {
            inst.keyboard_map[k].note = 60;
            inst.keyboard_map[k].sample = 4;
        }
    }

    for (int s = 1; s <= 4; ++s) {
        tracker::Sample smp;
        smp.name = "Smp " + std::to_string(s);
        smp.data8 = {static_cast<int8_t>(s * 10), static_cast<int8_t>(-s * 10)};
        smp.length = 2;
        inst.samples.push_back(std::move(smp));
    }
    song.instruments.push_back(std::move(inst));

    auto save_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& li = loaded.instruments[0];
    const auto& oi = song.instruments[0];

    REQUIRE_EQ(li.samples.size(), 4);
    for (size_t k = 0; k < 120; ++k) {
        REQUIRE_EQ(li.keyboard_map[k].note, oi.keyboard_map[k].note);
        REQUIRE_EQ(li.keyboard_map[k].sample, oi.keyboard_map[k].sample);
    }
}

TEST_CASE(ItRoundTrip_FileIOFidelity) {
    tracker::Song song;
    song.name = "IT File IO Test";
    song.message = "Writing to disk and reading back.";
    song.num_channels = 8;
    song.default_speed = 3;
    song.default_bpm = 150;
    song.linear_frequency = true;
    song.order_table = {0, 1};

    song.patterns.emplace_back(32, 8);
    song.patterns[0].get_cell(0, 0).note = 60;
    song.patterns[0].get_cell(0, 0).instrument = 1;
    song.patterns[0].get_cell(0, 0).volume = 0x40;
    song.patterns[0].get_cell(0, 0).effect_type = 0x06; // PitchSlideUp (F)
    song.patterns[0].get_cell(0, 0).effect_param = 0x0A;

    song.patterns.emplace_back(32, 8);
    song.patterns[1].get_cell(10, 5).note = tracker::it::IT_NOTE_OFF;

    song.instruments.emplace_back();
    auto& inst = song.instruments.back();
    inst.name = "Disk Lead";
    inst.volume_fadeout = 256;

    inst.samples.emplace_back();
    auto& s = inst.samples.back();
    s.name = "Saw Wave";
    s.volume = 64;
    s.c5_speed = 16726;
    s.is_16bit = false;
    s.loop_type = tracker::LoopType::Forward;
    s.data8 = {-60, -40, -20, 0, 20, 40, 60, 40, 20, 0, -20, -40};
    s.length = static_cast<uint32_t>(s.data8.size());
    s.loop_start = 0;
    s.loop_length = s.length;

    const std::string tmp_it = "test_it_roundtrip_file.it";
    auto save_status = tracker::it::ItWriter::save_to_file(song, tmp_it);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::it::ItReader::load_from_file(tmp_it);
    std::remove(tmp_it.c_str());

    REQUIRE(load_res.is_ok());
    const auto& loaded = load_res.value();

    REQUIRE_EQ(loaded.name, song.name);
    REQUIRE_EQ(loaded.message, song.message);
    REQUIRE_EQ(loaded.num_channels, 8);
    REQUIRE_EQ(loaded.default_speed, 3);
    REQUIRE_EQ(loaded.default_bpm, 150);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 60);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, 0x06);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x0A);
    REQUIRE_EQ(loaded.patterns[1].get_cell(10, 5).note, tracker::it::IT_NOTE_OFF);
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].name, "Disk Lead");
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples[0].data8.size(), s.data8.size());
    for (size_t i = 0; i < s.data8.size(); ++i) {
        REQUIRE_EQ(loaded.instruments[0].samples[0].data8[i], s.data8[i]);
    }
}

TEST_CASE(ItRoundTrip_AllEffectsAndSpecialNotes) {
    tracker::Song song;
    song.name = "All 26 IT Effects";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    // Place all 26 IT effects (commands 1..26)
    for (uint8_t cmd = 1; cmd <= 26; ++cmd) {
        auto& cell = song.patterns[0].get_cell(static_cast<uint16_t>(cmd - 1), 0);
        cell.note = static_cast<uint8_t>(20 + cmd);
        cell.instrument = 1;
        cell.volume = static_cast<uint8_t>(0x10 + cmd);
        cell.effect_type = cmd;
        cell.effect_param = static_cast<uint8_t>(0xA0 + cmd);
    }

    // Place special notes: Cut, Off, Fade
    song.patterns[0].get_cell(30, 1).note = tracker::it::IT_NOTE_CUT;
    song.patterns[0].get_cell(31, 1).note = tracker::it::IT_NOTE_OFF;
    song.patterns[0].get_cell(32, 1).note = tracker::it::IT_NOTE_FADE;

    song.instruments.emplace_back();
    song.instruments[0].name = "Test FX Inst";
    song.instruments[0].samples.emplace_back();
    song.instruments[0].samples[0].name = "FX Smp";
    song.instruments[0].samples[0].data8 = {10, -10};
    song.instruments[0].samples[0].length = 2;

    auto save_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    for (uint8_t cmd = 1; cmd <= 26; ++cmd) {
        const auto& cell = loaded.patterns[0].get_cell(static_cast<uint16_t>(cmd - 1), 0);
        REQUIRE_EQ(cell.note, static_cast<uint8_t>(20 + cmd));
        REQUIRE_EQ(cell.instrument, 1);
        REQUIRE_EQ(cell.volume, static_cast<uint8_t>(0x10 + cmd));
        REQUIRE_EQ(cell.effect_type, cmd);
        REQUIRE_EQ(cell.effect_param, static_cast<uint8_t>(0xA0 + cmd));
    }

    REQUIRE_EQ(loaded.patterns[0].get_cell(30, 1).note, tracker::it::IT_NOTE_CUT);
    REQUIRE_EQ(loaded.patterns[0].get_cell(31, 1).note, tracker::it::IT_NOTE_OFF);
    REQUIRE_EQ(loaded.patterns[0].get_cell(32, 1).note, tracker::it::IT_NOTE_FADE);
}

TEST_CASE(ItRoundTrip_SampleSustainLoopsAndPannings) {
    tracker::Song song;
    song.name = "Sustain Loops";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(32, 4);

    tracker::Instrument inst;
    inst.name = "Sustain Inst";

    for (size_t k = 0; k < 60; ++k) {
        inst.keyboard_map[k].note = static_cast<uint8_t>(k);
        inst.keyboard_map[k].sample = 1;
    }
    for (size_t k = 60; k < 120; ++k) {
        inst.keyboard_map[k].note = static_cast<uint8_t>(k);
        inst.keyboard_map[k].sample = 2;
    }

    // Sample 1: Forward Sustain Loop + PingPong Normal Loop (8-bit)
    {
        tracker::Sample s;
        s.name = "DualLoop 8b";
        s.volume = 58;
        s.global_volume = 50;
        s.panning = 32;
        s.c5_speed = 32000;
        s.is_16bit = false;
        s.loop_type = tracker::LoopType::PingPong;
        s.loop_start = 4;
        s.loop_length = 6;
        s.sustain_loop_type = tracker::LoopType::Forward;
        s.sustain_loop_start = 1;
        s.sustain_loop_length = 4;
        s.data8 = {0, 10, 20, 30, 40, 50, 40, 30, 20, 10, 0, -10};
        s.length = static_cast<uint32_t>(s.data8.size());
        inst.samples.push_back(std::move(s));
    }

    // Sample 2: PingPong Sustain Loop (16-bit)
    {
        tracker::Sample s;
        s.name = "PingPong Sus 16b";
        s.volume = 64;
        s.global_volume = 64;
        s.panning = 224;
        s.c5_speed = 48000;
        s.is_16bit = true;
        s.sustain_loop_type = tracker::LoopType::PingPong;
        s.sustain_loop_start = 2;
        s.sustain_loop_length = 8;
        s.data16 = {100, 500, 1000, 2000, 4000, 8000, 4000, 2000, 1000, 500, 100, -100};
        s.length = static_cast<uint32_t>(s.data16.size());
        inst.samples.push_back(std::move(s));
    }

    song.instruments.push_back(std::move(inst));

    auto save_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 2);

    const auto& s1 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(s1.name, "DualLoop 8b");
    REQUIRE_EQ(s1.volume, 58);
    REQUIRE_EQ(s1.global_volume, 50);
    REQUIRE_EQ(s1.panning, 32);
    REQUIRE_EQ(s1.c5_speed, 32000);
    REQUIRE(!s1.is_16bit);
    REQUIRE(s1.loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(s1.loop_start, 4);
    REQUIRE_EQ(s1.loop_length, 6);
    REQUIRE(s1.sustain_loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(s1.sustain_loop_start, 1);
    REQUIRE_EQ(s1.sustain_loop_length, 4);
    REQUIRE_EQ(s1.data8.size(), 12);

    const auto& s2 = loaded.instruments[0].samples[1];
    REQUIRE_EQ(s2.name, "PingPong Sus 16b");
    REQUIRE_EQ(s2.volume, 64);
    REQUIRE_EQ(s2.global_volume, 64);
    REQUIRE_EQ(s2.panning, 224);
    REQUIRE_EQ(s2.c5_speed, 48000);
    REQUIRE(s2.is_16bit);
    REQUIRE(s2.sustain_loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(s2.sustain_loop_start, 2);
    REQUIRE_EQ(s2.sustain_loop_length, 8);
    REQUIRE_EQ(s2.data16.size(), 12);
}
