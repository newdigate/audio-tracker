#include "test_main.hpp"
#include <tracker/s3m/s3m_writer.hpp>
#include <tracker/s3m/s3m_reader.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(S3mRoundTrip_FullSongFidelity) {
    tracker::Song original;
    original.name = "Scream Symphony 32";
    original.tracker_name = "Scream Tracker";
    original.version = 0x1320;
    original.num_channels = 32;
    original.default_speed = 6;
    original.default_bpm = 135;
    original.global_volume = 58;
    original.mix_volume = 44;
    original.linear_frequency = false;
    original.restart_position = 0;
    original.order_table = {0, 1, 2, 0, 255};

    for (size_t c = 0; c < 32; ++c) {
        original.channel_panning[c] = static_cast<uint8_t>((c * 4) % 64);
        original.channel_volume[c] = 64;
    }

    // Pattern 0: 64 rows, 32 channels - diverse cells across channels
    original.patterns.emplace_back(64, 32);
    for (uint16_t row = 0; row < 64; ++row) {
        for (uint16_t ch = 0; ch < 32; ++ch) {
            if ((row + ch) % 3 == 0) {
                auto& cell = original.patterns[0].get_cell(row, ch);
                cell.note = static_cast<uint8_t>(1 + (row * 2 + ch) % 96);
                cell.instrument = static_cast<uint8_t>(1 + (ch % 4));
                cell.volume = static_cast<uint8_t>(0x10 + ((row + ch) % 49)); // Volume 16..64
                cell.effect_type = static_cast<uint8_t>(1 + (ch % 24)); // S3M commands
                cell.effect_param = static_cast<uint8_t>((row * 3 + ch) & 0xFF);
            }
        }
    }

    // Special cell values
    original.patterns[0].get_cell(63, 0).note = 97; // Key Off (0xFE in S3M)
    original.patterns[0].get_cell(63, 0).instrument = 1;
    original.patterns[0].get_cell(63, 0).volume = 64;
    original.patterns[0].get_cell(63, 0).effect_type = static_cast<uint8_t>(tracker::s3m::S3mCommand::SetSpeed);
    original.patterns[0].get_cell(63, 0).effect_param = 4;

    original.patterns[0].get_cell(63, 31).note = 49; // C-4
    original.patterns[0].get_cell(63, 31).instrument = 2;
    original.patterns[0].get_cell(63, 31).volume = 48;
    original.patterns[0].get_cell(63, 31).effect_type = static_cast<uint8_t>(tracker::s3m::S3mCommand::SetTempo);
    original.patterns[0].get_cell(63, 31).effect_param = 140;

    // Pattern 1: 64 rows, 32 channels - partially populated
    original.patterns.emplace_back(64, 32);
    original.patterns[1].get_cell(0, 0).note = 49; // C-4
    original.patterns[1].get_cell(0, 0).instrument = 1;
    original.patterns[1].get_cell(0, 0).volume = 60;
    original.patterns[1].get_cell(16, 15).note = 61; // C-5
    original.patterns[1].get_cell(16, 15).instrument = 2;
    original.patterns[1].get_cell(32, 31).note = 97; // Key Off
    original.patterns[1].get_cell(32, 31).instrument = 4;

    // Pattern 2: 64 rows, 32 channels - completely empty
    original.patterns.emplace_back(64, 32);

    // Instrument 1: 8-bit PCM with Forward Loop
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Lead Synth";
        inst.filename = "LEAD.SMP";

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Lead Synth";
        s.volume = 56;
        s.c5_speed = 16726;
        s.is_16bit = false;
        s.loop_type = tracker::LoopType::Forward;
        s.data8 = {-50, -25, 0, 25, 50, 25, 0, -25, -50, -25, 0, 25, 50, 25, 0, -25};
        s.length = static_cast<uint32_t>(s.data8.size());
        s.loop_start = 2;
        s.loop_length = 8;
    }

    // Instrument 2: 16-bit PCM with Forward Loop
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "16bit Pad";
        inst.filename = "PAD16.SMP";

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "16bit Pad";
        s.volume = 62;
        s.c5_speed = 44100;
        s.is_16bit = true;
        s.loop_type = tracker::LoopType::Forward;
        s.data16 = {-12000, -6000, 0, 6000, 12000, 6000, 0, -6000, -12000, -6000, 0, 6000};
        s.length = static_cast<uint32_t>(s.data16.size());
        s.loop_start = 1;
        s.loop_length = 6;
    }

    // Instrument 3: Empty Instrument
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Empty Instrument";
        inst.filename = "";
    }

    // Instrument 4: 8-bit PCM Unlooped
    {
        original.instruments.emplace_back();
        auto& inst = original.instruments.back();
        inst.name = "Bass Drum";
        inst.filename = "DRUM.SMP";

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Bass Drum";
        s.volume = 64;
        s.c5_speed = 8363;
        s.is_16bit = false;
        s.loop_type = tracker::LoopType::None;
        s.data8 = {60, 40, 20, 0, -20, -40, -20, 0};
        s.length = static_cast<uint32_t>(s.data8.size());
    }

    // Save to memory
    auto save_res = tracker::s3m::S3mWriter::save_to_memory(original);
    REQUIRE(save_res.is_ok());

    const auto& s3m_bytes = save_res.value();
    REQUIRE(!s3m_bytes.empty());

    // Load back from memory
    auto load_res = tracker::s3m::S3mReader::load_from_memory(s3m_bytes.data(), s3m_bytes.size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();

    // Verify Metadata
    REQUIRE_EQ(loaded.name, original.name);
    REQUIRE_EQ(loaded.tracker_name, "Scream Tracker");
    REQUIRE_EQ(loaded.num_channels, 32);
    REQUIRE_EQ(loaded.default_speed, original.default_speed);
    REQUIRE_EQ(loaded.default_bpm, original.default_bpm);
    REQUIRE_EQ(loaded.global_volume, original.global_volume);
    REQUIRE_EQ(loaded.mix_volume, original.mix_volume);
    REQUIRE(!loaded.linear_frequency);

    // Verify Order Table
    REQUIRE_EQ(loaded.order_table.size(), original.order_table.size());
    for (size_t i = 0; i < original.order_table.size(); ++i) {
        REQUIRE_EQ(loaded.order_table[i], original.order_table[i]);
    }

    // Verify Channel Panning
    for (size_t c = 0; c < 32; ++c) {
        REQUIRE_EQ(loaded.channel_panning[c], original.channel_panning[c]);
    }

    // Verify Patterns
    REQUIRE_EQ(loaded.patterns.size(), 3);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[0].num_channels, 32);

    for (uint16_t r = 0; r < 64; ++r) {
        for (uint16_t c = 0; c < 32; ++c) {
            const auto& orig_cell = original.patterns[0].get_cell(r, c);
            const auto& load_cell = loaded.patterns[0].get_cell(r, c);
            REQUIRE_EQ(load_cell.note, orig_cell.note);
            REQUIRE_EQ(load_cell.instrument, orig_cell.instrument);
            REQUIRE_EQ(load_cell.volume, orig_cell.volume);
            REQUIRE_EQ(load_cell.effect_type, orig_cell.effect_type);
            REQUIRE_EQ(load_cell.effect_param, orig_cell.effect_param);
        }
    }

    // Verify Pattern 1 cells
    REQUIRE_EQ(loaded.patterns[1].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[1].num_channels, 32);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).note, 49);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(loaded.patterns[1].get_cell(0, 0).volume, 60);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 15).note, 61);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 15).instrument, 2);
    REQUIRE_EQ(loaded.patterns[1].get_cell(32, 31).note, 97);
    REQUIRE_EQ(loaded.patterns[1].get_cell(32, 31).instrument, 4);

    // Verify Pattern 2 empty
    REQUIRE_EQ(loaded.patterns[2].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[2].num_channels, 32);
    REQUIRE(loaded.patterns[2].is_all_empty());

    // Verify Instruments
    REQUIRE_EQ(loaded.instruments.size(), 4);

    // Instrument 1: 8-bit Loop
    {
        const auto& l_inst = loaded.instruments[0];
        const auto& o_inst = original.instruments[0];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE_EQ(l_inst.samples.size(), 1);

        const auto& ls = l_inst.samples[0];
        const auto& os = o_inst.samples[0];
        REQUIRE_EQ(ls.name, os.name);
        REQUIRE_EQ(ls.volume, os.volume);
        REQUIRE_EQ(ls.c5_speed, os.c5_speed);
        REQUIRE(!ls.is_16bit);
        REQUIRE(ls.loop_type == tracker::LoopType::Forward);
        REQUIRE_EQ(ls.loop_start, os.loop_start);
        REQUIRE_EQ(ls.loop_length, os.loop_length);
        REQUIRE_EQ(ls.data8.size(), os.data8.size());
        for (size_t d = 0; d < os.data8.size(); ++d) {
            REQUIRE_EQ(ls.data8[d], os.data8[d]);
        }
    }

    // Instrument 2: 16-bit Loop
    {
        const auto& l_inst = loaded.instruments[1];
        const auto& o_inst = original.instruments[1];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE_EQ(l_inst.samples.size(), 1);

        const auto& ls = l_inst.samples[0];
        const auto& os = o_inst.samples[0];
        REQUIRE_EQ(ls.name, os.name);
        REQUIRE_EQ(ls.volume, os.volume);
        REQUIRE_EQ(ls.c5_speed, os.c5_speed);
        REQUIRE(ls.is_16bit);
        REQUIRE(ls.loop_type == tracker::LoopType::Forward);
        REQUIRE_EQ(ls.loop_start, os.loop_start);
        REQUIRE_EQ(ls.loop_length, os.loop_length);
        REQUIRE_EQ(ls.data16.size(), os.data16.size());
        for (size_t d = 0; d < os.data16.size(); ++d) {
            REQUIRE_EQ(ls.data16[d], os.data16[d]);
        }
    }

    // Instrument 3: Empty
    {
        const auto& l_inst = loaded.instruments[2];
        REQUIRE_EQ(l_inst.samples.size(), 0);
    }

    // Instrument 4: 8-bit Unlooped
    {
        const auto& l_inst = loaded.instruments[3];
        const auto& o_inst = original.instruments[3];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE_EQ(l_inst.samples.size(), 1);

        const auto& ls = l_inst.samples[0];
        const auto& os = o_inst.samples[0];
        REQUIRE_EQ(ls.name, os.name);
        REQUIRE_EQ(ls.volume, os.volume);
        REQUIRE_EQ(ls.c5_speed, os.c5_speed);
        REQUIRE(!ls.is_16bit);
        REQUIRE(ls.loop_type == tracker::LoopType::None);
        REQUIRE_EQ(ls.data8.size(), os.data8.size());
        for (size_t d = 0; d < os.data8.size(); ++d) {
            REQUIRE_EQ(ls.data8[d], os.data8[d]);
        }
    }
}

TEST_CASE(S3mRoundTrip_FileIOFidelity) {
    tracker::Song song;
    song.name = "S3M File IO Song";
    song.tracker_name = "Scream Tracker";
    song.num_channels = 8;
    song.default_speed = 4;
    song.default_bpm = 145;
    song.global_volume = 64;
    song.mix_volume = 48;
    song.order_table = {0, 1};

    song.patterns.emplace_back(64, 8);
    song.patterns[0].get_cell(0, 0).note = 49;
    song.patterns[0].get_cell(0, 0).instrument = 1;
    song.patterns[0].get_cell(0, 0).volume = 0x30;
    song.patterns[0].get_cell(0, 0).effect_type = static_cast<uint8_t>(tracker::s3m::S3mCommand::PortamentoUp);
    song.patterns[0].get_cell(0, 0).effect_param = 0x05;

    song.patterns.emplace_back(64, 8);
    song.patterns[1].get_cell(16, 4).note = 97; // Key Off
    song.patterns[1].get_cell(16, 4).instrument = 1;

    song.instruments.emplace_back();
    auto& inst = song.instruments.back();
    inst.name = "File Lead";
    inst.filename = "LEAD.SMP";

    inst.samples.emplace_back();
    auto& s = inst.samples.back();
    s.name = "File Lead";
    s.volume = 58;
    s.c5_speed = 16726;
    s.is_16bit = false;
    s.loop_type = tracker::LoopType::Forward;
    s.data8 = {-64, -48, -32, -16, 0, 16, 32, 48, 64, 48, 32, 16, 0, -16, -32, -48};
    s.length = static_cast<uint32_t>(s.data8.size());
    s.loop_start = 0;
    s.loop_length = s.length;

    const std::string tmp_file = "test_s3m_roundtrip_file_temp.s3m";
    auto save_status = tracker::s3m::S3mWriter::save_to_file(song, tmp_file);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_file(tmp_file);
    std::remove(tmp_file.c_str());

    REQUIRE(load_res.is_ok());
    const auto& loaded = load_res.value();

    REQUIRE_EQ(loaded.name, song.name);
    REQUIRE_EQ(loaded.num_channels, 8);
    REQUIRE_EQ(loaded.default_speed, 4);
    REQUIRE_EQ(loaded.default_bpm, 145);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 49);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_type, static_cast<uint8_t>(tracker::s3m::S3mCommand::PortamentoUp));
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).effect_param, 0x05);
    REQUIRE_EQ(loaded.patterns[1].get_cell(16, 4).note, 97);
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].name, "File Lead");
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples[0].data8.size(), s.data8.size());
    for (size_t i = 0; i < s.data8.size(); ++i) {
        REQUIRE_EQ(loaded.instruments[0].samples[0].data8[i], s.data8[i]);
    }
}

TEST_CASE(S3mRoundTrip_AllEffectsAndSpecialNotes) {
    tracker::Song song;
    song.name = "All 26 S3M Effects";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    // Place all 26 effect commands (1..26)
    for (uint8_t cmd = 1; cmd <= 26; ++cmd) {
        auto& cell = song.patterns[0].get_cell(static_cast<uint16_t>(cmd - 1), 0);
        cell.note = static_cast<uint8_t>(13 + cmd);
        cell.instrument = 1;
        cell.volume = static_cast<uint8_t>(0x10 + (cmd % 48));
        cell.effect_type = cmd;
        cell.effect_param = static_cast<uint8_t>(cmd * 9);
    }

    // Place special notes: Note 1 (C-0), Note 49 (C-4), Note 97 (Key Off), Note 120 (B-9)
    song.patterns[0].get_cell(30, 1).note = 1;   // C-0
    song.patterns[0].get_cell(31, 1).note = 49;  // C-4
    song.patterns[0].get_cell(32, 1).note = 97;  // Key Off
    song.patterns[0].get_cell(33, 1).note = 120; // B-9

    song.instruments.emplace_back();
    song.instruments[0].name = "FX Sample";
    song.instruments[0].filename = "FX.SMP";
    song.instruments[0].samples.emplace_back();
    song.instruments[0].samples[0].name = "FX Sample";
    song.instruments[0].samples[0].data8 = {15, -15};
    song.instruments[0].samples[0].length = 2;
    song.instruments[0].samples[0].c5_speed = 8363;

    auto save_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    for (uint8_t cmd = 1; cmd <= 26; ++cmd) {
        const auto& cell = loaded.patterns[0].get_cell(static_cast<uint16_t>(cmd - 1), 0);
        REQUIRE_EQ(cell.note, static_cast<uint8_t>(13 + cmd));
        REQUIRE_EQ(cell.instrument, 1);
        REQUIRE_EQ(cell.volume, static_cast<uint8_t>(0x10 + (cmd % 48)));
        REQUIRE_EQ(cell.effect_type, cmd);
        REQUIRE_EQ(cell.effect_param, static_cast<uint8_t>(cmd * 9));
    }

    REQUIRE_EQ(loaded.patterns[0].get_cell(30, 1).note, 1);
    REQUIRE_EQ(loaded.patterns[0].get_cell(31, 1).note, 49);
    REQUIRE_EQ(loaded.patterns[0].get_cell(32, 1).note, 97);
    REQUIRE_EQ(loaded.patterns[0].get_cell(33, 1).note, 120);
}

TEST_CASE(S3mRoundTrip_MaxInstrumentsAnd32Channels) {
    tracker::Song song;
    song.name = "16 Insts 32 Chans";
    song.num_channels = 32;
    song.order_table = {0, 1};

    song.patterns.emplace_back(64, 32);
    song.patterns.emplace_back(64, 32);

    for (size_t c = 0; c < 32; ++c) {
        song.channel_panning[c] = static_cast<uint8_t>((c * 2) & 0x3C);
    }

    // 16 Instruments with varied sample rates, loop points, and volumes
    for (uint8_t i = 1; i <= 16; ++i) {
        song.instruments.emplace_back();
        auto& inst = song.instruments.back();
        inst.name = "Sample " + std::to_string(i);
        inst.filename = "SMP" + std::to_string(i) + ".SMP";

        inst.samples.emplace_back();
        auto& s = inst.samples.back();
        s.name = "Sample " + std::to_string(i);
        s.volume = static_cast<uint8_t>(20 + i * 2);
        s.c5_speed = static_cast<uint32_t>(8000 + i * 1000);
        s.is_16bit = (i % 3 == 0);

        if (i % 2 == 0) {
            s.loop_type = tracker::LoopType::Forward;
            s.loop_start = 2;
            s.loop_length = 4;
        } else {
            s.loop_type = tracker::LoopType::None;
            s.loop_start = 0;
            s.loop_length = 0;
        }

        if (s.is_16bit) {
            s.data16 = {static_cast<int16_t>(i * 500), static_cast<int16_t>(-i * 500), static_cast<int16_t>(i * 1000), static_cast<int16_t>(-i * 1000)};
            s.length = static_cast<uint32_t>(s.data16.size());
        } else {
            s.data8 = {static_cast<int8_t>(i * 4), static_cast<int8_t>(-i * 4), static_cast<int8_t>(i * 8), static_cast<int8_t>(-i * 8)};
            s.length = static_cast<uint32_t>(s.data8.size());
        }

        auto& cell = song.patterns[0].get_cell(i * 3, i - 1);
        cell.note = static_cast<uint8_t>(25 + i);
        cell.instrument = i;
        cell.volume = static_cast<uint8_t>(10 + i * 2);
    }

    auto save_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.num_channels, 32);
    REQUIRE_EQ(loaded.instruments.size(), 16);

    for (uint8_t i = 1; i <= 16; ++i) {
        const auto& o_inst = song.instruments[i - 1];
        const auto& l_inst = loaded.instruments[i - 1];
        REQUIRE_EQ(l_inst.name, o_inst.name);
        REQUIRE_EQ(l_inst.filename, o_inst.filename);
        REQUIRE_EQ(l_inst.samples.size(), 1);

        const auto& os = o_inst.samples[0];
        const auto& ls = l_inst.samples[0];
        REQUIRE_EQ(ls.volume, os.volume);
        REQUIRE_EQ(ls.c5_speed, os.c5_speed);
        REQUIRE_EQ(ls.is_16bit, os.is_16bit);
        REQUIRE(ls.loop_type == os.loop_type);
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

        const auto& cell = loaded.patterns[0].get_cell(i * 3, i - 1);
        REQUIRE_EQ(cell.note, static_cast<uint8_t>(25 + i));
        REQUIRE_EQ(cell.instrument, i);
        REQUIRE_EQ(cell.volume, static_cast<uint8_t>(10 + i * 2));
    }
}

TEST_CASE(S3mRoundTrip_AudioPcmDataFidelity) {
    tracker::Song song;
    song.name = "Audio PCM Fidelity";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    // 8-bit sample with full dynamic range edge cases
    song.instruments.emplace_back();
    song.instruments[0].name = "8bit Extremes";
    song.instruments[0].filename = "8BIT.SMP";
    song.instruments[0].samples.emplace_back();
    auto& s8 = song.instruments[0].samples.back();
    s8.name = "8bit Extremes";
    s8.volume = 64;
    s8.c5_speed = 22050;
    s8.is_16bit = false;
    s8.loop_type = tracker::LoopType::Forward;
    s8.data8 = {
        static_cast<int8_t>(-128), static_cast<int8_t>(-127), -64, -1,
        0, 1, 64, 127
    };
    s8.length = static_cast<uint32_t>(s8.data8.size());
    s8.loop_start = 0;
    s8.loop_length = s8.length;

    // 16-bit sample with full dynamic range edge cases
    song.instruments.emplace_back();
    song.instruments[1].name = "16bit Extremes";
    song.instruments[1].filename = "16BIT.SMP";
    song.instruments[1].samples.emplace_back();
    auto& s16 = song.instruments[1].samples.back();
    s16.name = "16bit Extremes";
    s16.volume = 64;
    s16.c5_speed = 44100;
    s16.is_16bit = true;
    s16.loop_type = tracker::LoopType::Forward;
    s16.data16 = {
        static_cast<int16_t>(-32768), static_cast<int16_t>(-32767), -16384, -1,
        0, 1, 16384, 32767
    };
    s16.length = static_cast<uint32_t>(s16.data16.size());
    s16.loop_start = 0;
    s16.loop_length = s16.length;

    auto save_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 2);

    // Assert 8-bit fidelity
    const auto& ls8 = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls8.data8.size(), s8.data8.size());
    for (size_t i = 0; i < s8.data8.size(); ++i) {
        REQUIRE_EQ(ls8.data8[i], s8.data8[i]);
    }

    // Assert 16-bit fidelity
    const auto& ls16 = loaded.instruments[1].samples[0];
    REQUIRE_EQ(ls16.data16.size(), s16.data16.size());
    for (size_t i = 0; i < s16.data16.size(); ++i) {
        REQUIRE_EQ(ls16.data16[i], s16.data16[i]);
    }
}

TEST_CASE(S3mRoundTrip_ParapointerParagraphAlignment) {
    tracker::Song song;
    song.name = "Unaligned Data Alignment";
    song.num_channels = 4;
    song.order_table = {0, 1, 2};

    // Pattern sizes that are not multiples of 16 bytes
    song.patterns.emplace_back(64, 4);
    song.patterns[0].get_cell(0, 0).note = 25;
    song.patterns[0].get_cell(0, 0).instrument = 1;

    song.patterns.emplace_back(64, 4);
    song.patterns[1].get_cell(1, 1).note = 37;
    song.patterns[1].get_cell(1, 1).instrument = 1;
    song.patterns[1].get_cell(2, 2).volume = 40;

    song.patterns.emplace_back(64, 4);
    song.patterns[2].get_cell(10, 3).effect_type = 1;
    song.patterns[2].get_cell(10, 3).effect_param = 6;

    // Sample sizes that are odd numbers (e.g. 7 bytes and 11 words)
    song.instruments.emplace_back();
    song.instruments[0].name = "Odd Smp 8b";
    song.instruments[0].samples.emplace_back();
    auto& s1 = song.instruments[0].samples.back();
    s1.name = "Odd Smp 8b";
    s1.data8 = {1, 2, 3, 4, 5, 6, 7};
    s1.length = 7;

    song.instruments.emplace_back();
    song.instruments[1].name = "Odd Smp 16b";
    song.instruments[1].samples.emplace_back();
    auto& s2 = song.instruments[1].samples.back();
    s2.name = "Odd Smp 16b";
    s2.is_16bit = true;
    s2.data16 = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100};
    s2.length = 11;

    auto save_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(save_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(save_res.value().data(), save_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.patterns.size(), 3);
    REQUIRE_EQ(loaded.patterns[0].get_cell(0, 0).note, 25);
    REQUIRE_EQ(loaded.patterns[1].get_cell(1, 1).note, 37);
    REQUIRE_EQ(loaded.patterns[1].get_cell(2, 2).volume, 40);
    REQUIRE_EQ(loaded.patterns[2].get_cell(10, 3).effect_type, 1);
    REQUIRE_EQ(loaded.patterns[2].get_cell(10, 3).effect_param, 6);

    REQUIRE_EQ(loaded.instruments.size(), 2);
    REQUIRE_EQ(loaded.instruments[0].samples[0].data8.size(), 7);
    for (size_t i = 0; i < 7; ++i) {
        REQUIRE_EQ(loaded.instruments[0].samples[0].data8[i], s1.data8[i]);
    }
    REQUIRE_EQ(loaded.instruments[1].samples[0].data16.size(), 11);
    for (size_t i = 0; i < 11; ++i) {
        REQUIRE_EQ(loaded.instruments[1].samples[0].data16[i], s2.data16[i]);
    }
}
