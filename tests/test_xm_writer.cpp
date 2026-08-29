#include "test_main.hpp"
#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_reader.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(XmWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test Export";
    song.num_channels = 4;
    song.order_table = {0, 0};
    song.patterns.emplace_back(64, 4);

    auto res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    const auto& bytes = res.value();
    REQUIRE(bytes.size() > 60);

    // Verify signature
    std::string sig(reinterpret_cast<const char*>(bytes.data()), 17);
    REQUIRE_EQ(sig, "Extended Module: ");
}

TEST_CASE(XmWriter_RoundTrip_Minimal) {
    tracker::Song song;
    song.name = "Minimal Song";
    song.tracker_name = "FastTracker v2.00";
    song.num_channels = 4;
    song.default_speed = 6;
    song.default_bpm = 125;
    song.restart_position = 0;
    song.linear_frequency = true;
    song.order_table = {0, 1};
    song.patterns.emplace_back(64, 4);
    song.patterns.emplace_back(32, 4);

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.name, "Minimal Song");
    REQUIRE_EQ(loaded.tracker_name, "FastTracker v2.00");
    REQUIRE_EQ(loaded.num_channels, 4);
    REQUIRE_EQ(loaded.default_speed, 6);
    REQUIRE_EQ(loaded.default_bpm, 125);
    REQUIRE_EQ(loaded.restart_position, 0);
    REQUIRE(loaded.linear_frequency);
    REQUIRE_EQ(loaded.order_table.size(), 2);
    REQUIRE_EQ(loaded.order_table[0], 0);
    REQUIRE_EQ(loaded.order_table[1], 1);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[1].num_rows, 32);
    REQUIRE(loaded.patterns[0].is_all_empty());
    REQUIRE(loaded.patterns[1].is_all_empty());
    REQUIRE_EQ(loaded.instruments.size(), 0);
}

TEST_CASE(XmWriter_RoundTrip_Patterns) {
    tracker::Song song;
    song.name = "Pattern Test";
    song.num_channels = 2;
    song.order_table = {0};

    tracker::Pattern pat(4, 2);
    pat.get_cell(0, 0).note = 49;
    pat.get_cell(0, 0).instrument = 1;
    pat.get_cell(0, 0).volume = 0x40;
    pat.get_cell(0, 0).effect_type = 0x01;
    pat.get_cell(0, 0).effect_param = 0x03;

    pat.get_cell(1, 1).note = 97; // Key off
    pat.get_cell(1, 1).effect_type = 0x0A;
    pat.get_cell(1, 1).effect_param = 0x0F;

    pat.get_cell(3, 0).instrument = 2;
    pat.get_cell(3, 1).volume = 0x20;

    song.patterns.push_back(std::move(pat));

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.patterns.size(), 1);
    const auto& p = loaded.patterns[0];
    REQUIRE_EQ(p.num_rows, 4);
    REQUIRE_EQ(p.num_channels, 2);

    REQUIRE_EQ(p.get_cell(0, 0).note, 49);
    REQUIRE_EQ(p.get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(p.get_cell(0, 0).volume, 0x40);
    REQUIRE_EQ(p.get_cell(0, 0).effect_type, 0x01);
    REQUIRE_EQ(p.get_cell(0, 0).effect_param, 0x03);

    REQUIRE_EQ(p.get_cell(1, 1).note, 97);
    REQUIRE_EQ(p.get_cell(1, 1).instrument, 0);
    REQUIRE_EQ(p.get_cell(1, 1).effect_type, 0x0A);
    REQUIRE_EQ(p.get_cell(1, 1).effect_param, 0x0F);

    REQUIRE_EQ(p.get_cell(3, 0).instrument, 2);
    REQUIRE_EQ(p.get_cell(3, 1).volume, 0x20);
    REQUIRE(p.get_cell(2, 0).is_empty());
    REQUIRE(p.get_cell(2, 1).is_empty());
}

TEST_CASE(XmWriter_RoundTrip_EmptyInstrument) {
    tracker::Song song;
    song.name = "Empty Inst Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Empty Instrument";
    inst.type = 0;
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].name, "Empty Instrument");
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 0);
}

TEST_CASE(XmWriter_RoundTrip_8BitSampleInstrument) {
    tracker::Song song;
    song.name = "8bit Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Synth Bass";
    inst.vibrato_type = 1;
    inst.vibrato_sweep = 10;
    inst.vibrato_depth = 5;
    inst.vibrato_rate = 20;
    inst.volume_fadeout = 512;

    inst.volume_envelope.enabled = true;
    inst.volume_envelope.sustain_enabled = true;
    inst.volume_envelope.loop_enabled = true;
    inst.volume_envelope.sustain_point = 2;
    inst.volume_envelope.loop_start_point = 0;
    inst.volume_envelope.loop_end_point = 3;
    inst.volume_envelope.points = {
        {0, 64}, {10, 48}, {20, 32}, {40, 0}
    };

    inst.panning_envelope.enabled = true;
    inst.panning_envelope.sustain_enabled = false;
    inst.panning_envelope.loop_enabled = false;
    inst.panning_envelope.points = {
        {0, 32}, {20, 48}
    };

    tracker::Sample s;
    s.name = "Bass Sample";
    s.data8 = {10, 20, 30, 40, 50, 40, 30, 20, 10, 0, -10, -20};
    s.length = static_cast<uint32_t>(s.data8.size());
    s.loop_type = tracker::LoopType::Forward;
    s.loop_start = 2;
    s.loop_length = 6;
    s.volume = 58;
    s.finetune = 3;
    s.panning = 140;
    s.relative_note = -5;
    s.is_16bit = false;

    inst.samples.push_back(std::move(s));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& loaded_inst = loaded.instruments[0];
    REQUIRE_EQ(loaded_inst.name, "Synth Bass");
    REQUIRE_EQ(loaded_inst.vibrato_type, 1);
    REQUIRE_EQ(loaded_inst.vibrato_sweep, 10);
    REQUIRE_EQ(loaded_inst.vibrato_depth, 5);
    REQUIRE_EQ(loaded_inst.vibrato_rate, 20);
    REQUIRE_EQ(loaded_inst.volume_fadeout, 512);

    REQUIRE(loaded_inst.volume_envelope.enabled);
    REQUIRE(loaded_inst.volume_envelope.sustain_enabled);
    REQUIRE(loaded_inst.volume_envelope.loop_enabled);
    REQUIRE_EQ(loaded_inst.volume_envelope.sustain_point, 2);
    REQUIRE_EQ(loaded_inst.volume_envelope.loop_start_point, 0);
    REQUIRE_EQ(loaded_inst.volume_envelope.loop_end_point, 3);
    REQUIRE_EQ(loaded_inst.volume_envelope.points.size(), 4);
    REQUIRE_EQ(loaded_inst.volume_envelope.points[0].tick, 0);
    REQUIRE_EQ(loaded_inst.volume_envelope.points[0].value, 64);
    REQUIRE_EQ(loaded_inst.volume_envelope.points[1].tick, 10);
    REQUIRE_EQ(loaded_inst.volume_envelope.points[1].value, 48);

    REQUIRE(loaded_inst.panning_envelope.enabled);
    REQUIRE(!loaded_inst.panning_envelope.sustain_enabled);
    REQUIRE(!loaded_inst.panning_envelope.loop_enabled);
    REQUIRE_EQ(loaded_inst.panning_envelope.points.size(), 2);
    REQUIRE_EQ(loaded_inst.panning_envelope.points[1].tick, 20);
    REQUIRE_EQ(loaded_inst.panning_envelope.points[1].value, 48);

    REQUIRE_EQ(loaded_inst.samples.size(), 1);
    const auto& loaded_s = loaded_inst.samples[0];
    REQUIRE_EQ(loaded_s.name, "Bass Sample");
    REQUIRE(!loaded_s.is_16bit);
    REQUIRE_EQ(loaded_s.length, 12);
    REQUIRE(loaded_s.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(loaded_s.loop_start, 2);
    REQUIRE_EQ(loaded_s.loop_length, 6);
    REQUIRE_EQ(loaded_s.volume, 58);
    REQUIRE_EQ(loaded_s.finetune, 3);
    REQUIRE_EQ(loaded_s.panning, 140);
    REQUIRE_EQ(loaded_s.relative_note, -5);
    REQUIRE_EQ(loaded_s.data8.size(), 12);
    for (size_t i = 0; i < loaded_s.data8.size(); ++i) {
        REQUIRE_EQ(loaded_s.data8[i], (std::vector<int8_t>{10, 20, 30, 40, 50, 40, 30, 20, 10, 0, -10, -20})[i]);
    }
}

TEST_CASE(XmWriter_RoundTrip_16BitSampleInstrument) {
    tracker::Song song;
    song.name = "16bit Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Acoustic Piano";

    tracker::Sample s;
    s.name = "Piano Sample";
    s.data16 = {1000, 2000, -3000, 4000, -5000, 6000};
    s.length = static_cast<uint32_t>(s.data16.size());
    s.loop_type = tracker::LoopType::PingPong;
    s.loop_start = 1;
    s.loop_length = 4;
    s.volume = 60;
    s.finetune = -4;
    s.panning = 96;
    s.relative_note = 12;
    s.is_16bit = true;

    inst.samples.push_back(std::move(s));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& loaded_inst = loaded.instruments[0];
    REQUIRE_EQ(loaded_inst.name, "Acoustic Piano");

    REQUIRE_EQ(loaded_inst.samples.size(), 1);
    const auto& loaded_s = loaded_inst.samples[0];
    REQUIRE_EQ(loaded_s.name, "Piano Sample");
    REQUIRE(loaded_s.is_16bit);
    REQUIRE(loaded_s.loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(loaded_s.length, 6);
    REQUIRE_EQ(loaded_s.loop_start, 1);
    REQUIRE_EQ(loaded_s.loop_length, 4);
    REQUIRE_EQ(loaded_s.volume, 60);
    REQUIRE_EQ(loaded_s.finetune, -4);
    REQUIRE_EQ(loaded_s.panning, 96);
    REQUIRE_EQ(loaded_s.relative_note, 12);
    REQUIRE_EQ(loaded_s.data16.size(), 6);
    for (size_t i = 0; i < loaded_s.data16.size(); ++i) {
        REQUIRE_EQ(loaded_s.data16[i], (std::vector<int16_t>{1000, 2000, -3000, 4000, -5000, 6000})[i]);
    }
}

TEST_CASE(XmWriter_RoundTrip_MultiSampleInstrument) {
    tracker::Song song;
    song.name = "MultiSample Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Drum Kit";
    for (size_t i = 0; i < 48; ++i) inst.sample_map[i] = 0;
    for (size_t i = 48; i < 96; ++i) inst.sample_map[i] = 1;

    // Sample 0 (8-bit Kick)
    tracker::Sample kick;
    kick.name = "Kick 8bit";
    kick.data8 = {50, 40, 30, 20, 10, 0};
    kick.length = static_cast<uint32_t>(kick.data8.size());
    kick.loop_type = tracker::LoopType::None;
    kick.volume = 64;
    kick.panning = 128;
    kick.is_16bit = false;

    // Sample 1 (16-bit Snare)
    tracker::Sample snare;
    snare.name = "Snare 16bit";
    snare.data16 = {10000, -8000, 5000, -2000, 500};
    snare.length = static_cast<uint32_t>(snare.data16.size());
    snare.loop_type = tracker::LoopType::None;
    snare.volume = 60;
    snare.panning = 200;
    snare.is_16bit = true;

    inst.samples.push_back(std::move(kick));
    inst.samples.push_back(std::move(snare));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& loaded_inst = loaded.instruments[0];
    REQUIRE_EQ(loaded_inst.name, "Drum Kit");
    REQUIRE_EQ(loaded_inst.sample_map[0], 0);
    REQUIRE_EQ(loaded_inst.sample_map[47], 0);
    REQUIRE_EQ(loaded_inst.sample_map[48], 1);
    REQUIRE_EQ(loaded_inst.sample_map[95], 1);
    REQUIRE_EQ(loaded_inst.samples.size(), 2);

    REQUIRE_EQ(loaded_inst.samples[0].name, "Kick 8bit");
    REQUIRE(!loaded_inst.samples[0].is_16bit);
    REQUIRE_EQ(loaded_inst.samples[0].data8.size(), 6);

    REQUIRE_EQ(loaded_inst.samples[1].name, "Snare 16bit");
    REQUIRE(loaded_inst.samples[1].is_16bit);
    REQUIRE_EQ(loaded_inst.samples[1].data16.size(), 5);
}

TEST_CASE(XmWriter_RoundTrip_AmigaFrequencyAndFullOrderTable) {
    tracker::Song song;
    song.name = "Amiga Full Orders";
    song.num_channels = 8;
    song.linear_frequency = false; // Amiga frequency table
    song.restart_position = 42;
    song.default_speed = 3;
    song.default_bpm = 145;

    song.order_table.resize(256);
    for (size_t i = 0; i < 256; ++i) {
        song.order_table[i] = static_cast<uint8_t>(i % 4);
    }

    for (int i = 0; i < 4; ++i) {
        song.patterns.emplace_back(32, 8);
    }

    auto write_res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    const auto& bytes = write_res.value();
    auto read_res = tracker::xm::XmReader::load_from_memory(bytes.data(), bytes.size());
    REQUIRE(read_res.is_ok());

    const auto& loaded = read_res.value();
    REQUIRE(!loaded.linear_frequency);
    REQUIRE_EQ(loaded.restart_position, 42);
    REQUIRE_EQ(loaded.default_speed, 3);
    REQUIRE_EQ(loaded.default_bpm, 145);
    REQUIRE_EQ(loaded.num_channels, 8);
    REQUIRE_EQ(loaded.order_table.size(), 256);
    for (size_t i = 0; i < 256; ++i) {
        REQUIRE_EQ(loaded.order_table[i], static_cast<uint8_t>(i % 4));
    }
    REQUIRE_EQ(loaded.patterns.size(), 4);
}

TEST_CASE(XmWriter_SaveToFile_And_LoadFromFile) {
    tracker::Song song;
    song.name = "File Save Test";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    const std::string tmp_file = "test_writer_temp.xm";
    auto save_status = tracker::xm::XmWriter::save_to_file(song, tmp_file);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_file(tmp_file);
    std::remove(tmp_file.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File Save Test");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(XmWriter_SaveToFile_Error) {
    tracker::Song song;
    // Attempt to write to invalid path (e.g. non-existent directory)
    auto status = tracker::xm::XmWriter::save_to_file(song, "/non_existent_dir_12345/test.xm");
    REQUIRE(!status.is_ok());
    REQUIRE_EQ(status.code, tracker::ErrorCode::IoError);
}

TEST_CASE(XmWriter_OrderTableClamping) {
    tracker::Song song;
    song.name = "Clamped Orders";
    song.num_channels = 4;
    song.order_table.resize(300);
    for (size_t i = 0; i < 300; ++i) {
        song.order_table[i] = static_cast<uint8_t>(i & 0xFF);
    }
    song.patterns.emplace_back(64, 4);

    auto res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().order_table.size(), 256);
    for (size_t i = 0; i < 256; ++i) {
        REQUIRE_EQ(load_res.value().order_table[i], static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE(XmWriter_DefaultTrackerName) {
    tracker::Song song;
    song.name = "No Tracker Name";
    song.tracker_name = ""; // Empty
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    auto res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().tracker_name, "FastTracker v2.00");
}

TEST_CASE(XmWriter_MaxEnvelopePointsClamped) {
    tracker::Song song;
    song.name = "Env Clamped";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Many Points";
    for (uint16_t i = 0; i < 20; ++i) {
        inst.volume_envelope.points.push_back({static_cast<uint16_t>(i * 5), static_cast<uint16_t>(i)});
        inst.panning_envelope.points.push_back({static_cast<uint16_t>(i * 5), 32});
    }

    tracker::Sample s;
    s.name = "Dummy Sample";
    s.data8 = {1, 2, 3};
    s.length = 3;
    inst.samples.push_back(std::move(s));
    song.instruments.push_back(std::move(inst));

    auto res = tracker::xm::XmWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::xm::XmReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().instruments[0].volume_envelope.points.size(), 12);
    REQUIRE_EQ(load_res.value().instruments[0].panning_envelope.points.size(), 12);
}

TEST_CASE(XmWriter_DirectStreamSave) {
    tracker::Song song;
    song.name = "Stream Save";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::io::MemoryOutputStream stream;
    auto status = tracker::xm::XmWriter::save(song, stream);
    REQUIRE(status.is_ok());
    REQUIRE(stream.data().size() > 0);

    auto load_res = tracker::xm::XmReader::load_from_memory(stream.data().data(), stream.data().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Stream Save");
}

