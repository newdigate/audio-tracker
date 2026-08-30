#include "test_main.hpp"
#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_reader.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(ItWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test IT Song";
    song.num_channels = 8;
    song.order_table = {0};
    song.patterns.emplace_back(64, 8);

    auto res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Test IT Song");
    REQUIRE_EQ(load_res.value().num_channels, 8);
}

TEST_CASE(ItWriter_SongMetadata) {
    tracker::Song song;
    song.name = "Metadata Song";
    song.message = "Hello IT Message!\r\nLine two.";
    song.num_channels = 6;
    song.default_speed = 4;
    song.default_bpm = 140;
    song.global_volume = 110;
    song.mix_volume = 40;
    song.pan_separation = 100;
    song.linear_frequency = true;
    song.order_table = {0, 1, 0, 255};
    song.patterns.emplace_back(64, 6);
    song.patterns.emplace_back(32, 6);

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, "Metadata Song");
    REQUIRE_EQ(loaded.message, "Hello IT Message!\r\nLine two.");
    REQUIRE_EQ(loaded.num_channels, 6);
    REQUIRE_EQ(loaded.default_speed, 4);
    REQUIRE_EQ(loaded.default_bpm, 140);
    REQUIRE_EQ(loaded.global_volume, 110);
    REQUIRE_EQ(loaded.mix_volume, 40);
    REQUIRE_EQ(loaded.pan_separation, 100);
    REQUIRE(loaded.linear_frequency);
    REQUIRE_EQ(loaded.order_table.size(), 4);
    REQUIRE_EQ(loaded.order_table[0], 0);
    REQUIRE_EQ(loaded.order_table[1], 1);
    REQUIRE_EQ(loaded.order_table[2], 0);
    REQUIRE_EQ(loaded.order_table[3], 255);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[1].num_rows, 32);
}

TEST_CASE(ItWriter_Sample_8BitUncompressed) {
    tracker::Song song;
    song.name = "8Bit Audio";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Bass";

    tracker::Sample smp;
    smp.name = "Bass Wave";
    smp.data8 = {0, 30, 60, 30, 0, -30, -60, -30};
    smp.length = static_cast<uint32_t>(smp.data8.size());
    smp.volume = 45;
    smp.global_volume = 50;
    smp.panning = 64;
    smp.c5_speed = 16726;
    smp.loop_type = tracker::LoopType::Forward;
    smp.loop_start = 2;
    smp.loop_length = 4;
    smp.vibrato_sweep = 10;
    smp.vibrato_depth = 20;
    smp.vibrato_rate = 30;
    smp.vibrato_type = 1;
    smp.is_16bit = false;

    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    const auto& ls = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls.name, "Bass Wave");
    REQUIRE(!ls.is_16bit);
    REQUIRE_EQ(ls.length, 8);
    REQUIRE_EQ(ls.volume, 45);
    REQUIRE_EQ(ls.global_volume, 50);
    REQUIRE_EQ(ls.panning, 64);
    REQUIRE_EQ(ls.c5_speed, 16726);
    REQUIRE(ls.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(ls.loop_start, 2);
    REQUIRE_EQ(ls.loop_length, 4);
    REQUIRE_EQ(ls.vibrato_sweep, 10);
    REQUIRE_EQ(ls.vibrato_depth, 20);
    REQUIRE_EQ(ls.vibrato_rate, 30);
    REQUIRE_EQ(ls.vibrato_type, 1);
    REQUIRE_EQ(ls.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(ls.data8[i], (std::vector<int8_t>{0, 30, 60, 30, 0, -30, -60, -30})[i]);
    }
}

TEST_CASE(ItWriter_Sample_16BitUncompressed) {
    tracker::Song song;
    song.name = "16Bit Audio";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Lead";

    tracker::Sample smp;
    smp.name = "HiFi Lead";
    smp.data16 = {1000, 2000, -2000, -1000};
    smp.length = static_cast<uint32_t>(smp.data16.size());
    smp.volume = 60;
    smp.global_volume = 64;
    smp.panning = 128;
    smp.c5_speed = 44100;
    smp.sustain_loop_type = tracker::LoopType::PingPong;
    smp.sustain_loop_start = 1;
    smp.sustain_loop_length = 2;
    smp.is_16bit = true;

    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    const auto& ls = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls.name, "HiFi Lead");
    REQUIRE(ls.is_16bit);
    REQUIRE_EQ(ls.length, 4);
    REQUIRE_EQ(ls.c5_speed, 44100);
    REQUIRE(ls.sustain_loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(ls.sustain_loop_start, 1);
    REQUIRE_EQ(ls.sustain_loop_length, 2);
    REQUIRE_EQ(ls.data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(ls.data16[i], (std::vector<int16_t>{1000, 2000, -2000, -1000})[i]);
    }
}

TEST_CASE(ItWriter_InstrumentMode_ComplexEnvelopes) {
    tracker::Song song;
    song.name = "Full Instrument Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Complex Inst";
    inst.filename = "COMPLEX.ITI";
    inst.nna = tracker::NewNoteAction::NoteOff;
    inst.dct = tracker::DuplicateCheckType::Sample;
    inst.dca = tracker::DuplicateCheckAction::NoteFade;
    inst.volume_fadeout = 512;
    inst.global_volume = 100;
    inst.default_panning = 128;

    inst.volume_envelope.enabled = true;
    inst.volume_envelope.loop_enabled = true;
    inst.volume_envelope.sustain_enabled = true;
    inst.volume_envelope.loop_start_point = 0;
    inst.volume_envelope.loop_end_point = 1;
    inst.volume_envelope.sustain_point = 0;
    inst.volume_envelope.points = {
        {0, 64},
        {50, 0}
    };

    inst.pitch_envelope.enabled = true;
    inst.pitch_envelope.points = {
        {10, 32}
    };

    tracker::Sample smp;
    smp.name = "Inst Sample";
    smp.data8 = {10, -10};
    smp.length = 2;

    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& li = loaded.instruments[0];
    REQUIRE_EQ(li.name, "Complex Inst");
    REQUIRE_EQ(li.filename, "COMPLEX.ITI");
    REQUIRE(li.nna == tracker::NewNoteAction::NoteOff);
    REQUIRE(li.dct == tracker::DuplicateCheckType::Sample);
    REQUIRE(li.dca == tracker::DuplicateCheckAction::NoteFade);
    REQUIRE_EQ(li.volume_fadeout, 512);
    REQUIRE_EQ(li.global_volume, 100);

    REQUIRE(li.volume_envelope.enabled);
    REQUIRE(li.volume_envelope.loop_enabled);
    REQUIRE(li.volume_envelope.sustain_enabled);
    REQUIRE_EQ(li.volume_envelope.points.size(), 2);
    REQUIRE_EQ(li.volume_envelope.points[0].tick, 0);
    REQUIRE_EQ(li.volume_envelope.points[0].value, 64);
    REQUIRE_EQ(li.volume_envelope.points[1].tick, 50);
    REQUIRE_EQ(li.volume_envelope.points[1].value, 0);

    REQUIRE(li.pitch_envelope.enabled);
    REQUIRE_EQ(li.pitch_envelope.points.size(), 1);
    REQUIRE_EQ(li.pitch_envelope.points[0].tick, 10);
    REQUIRE_EQ(li.pitch_envelope.points[0].value, 32);

    REQUIRE_EQ(li.samples.size(), 1);
    REQUIRE_EQ(li.samples[0].data8.size(), 2);
    REQUIRE_EQ(li.samples[0].data8[0], 10);
    REQUIRE_EQ(li.samples[0].data8[1], -10);
}

TEST_CASE(ItWriter_PatternData) {
    tracker::Song song;
    song.name = "Pattern Test";
    song.num_channels = 4;
    song.order_table = {0};

    tracker::Pattern pat(64, 4);
    auto& c0 = pat.get_cell(0, 0);
    c0.note = 49; // C-4
    c0.instrument = 1;
    c0.volume = 0x50;
    c0.effect_type = 1;
    c0.effect_param = 6;

    auto& c1 = pat.get_cell(2, 3);
    c1.note = tracker::it::IT_NOTE_CUT;
    c1.instrument = 2;

    song.patterns.push_back(std::move(pat));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.patterns.size(), 1);
    const auto& p = loaded.patterns[0];
    REQUIRE_EQ(p.num_rows, 64);
    REQUIRE_EQ(p.num_channels, 4);

    const auto& lc0 = p.get_cell(0, 0);
    REQUIRE_EQ(lc0.note, 49);
    REQUIRE_EQ(lc0.instrument, 1);
    REQUIRE_EQ(lc0.volume, 0x50);
    REQUIRE_EQ(lc0.effect_type, 1);
    REQUIRE_EQ(lc0.effect_param, 6);

    const auto& lc1 = p.get_cell(2, 3);
    REQUIRE_EQ(lc1.note, tracker::it::IT_NOTE_CUT);
    REQUIRE_EQ(lc1.instrument, 2);
}

TEST_CASE(ItWriter_DirectStreamSave) {
    tracker::Song song;
    song.name = "Stream Save";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::io::MemoryOutputStream stream;
    auto status = tracker::it::ItWriter::save(song, stream);
    REQUIRE(status.is_ok());
    REQUIRE(stream.data().size() > 0);

    auto load_res = tracker::it::ItReader::load_from_memory(stream.data().data(), stream.data().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Stream Save");
}

TEST_CASE(ItWriter_SaveToFile_And_LoadFromFile) {
    tracker::Song song;
    song.name = "File Save Test";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    const std::string tmp_file = "test_it_writer_temp.it";
    auto save_status = tracker::it::ItWriter::save_to_file(song, tmp_file);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::it::ItReader::load_from_file(tmp_file);
    std::remove(tmp_file.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File Save Test");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(ItWriter_SaveToFile_Error) {
    tracker::Song song;
    auto status = tracker::it::ItWriter::save_to_file(song, "/non_existent_dir_12345/test.it");
    REQUIRE(!status.is_ok());
    REQUIRE_EQ(status.code, tracker::ErrorCode::IoError);
}

TEST_CASE(ItWriter_OrderTableClamping) {
    tracker::Song song;
    song.name = "Clamped Orders";
    song.num_channels = 4;
    song.order_table.resize(300);
    for (size_t i = 0; i < 300; ++i) {
        song.order_table[i] = static_cast<uint8_t>(i & 0xFF);
    }
    song.patterns.emplace_back(64, 4);

    auto res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().order_table.size(), 256);
    for (size_t i = 0; i < 256; ++i) {
        REQUIRE_EQ(load_res.value().order_table[i], static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE(ItWriter_MultiSampleInstrument) {
    tracker::Song song;
    song.name = "MultiSample Song";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Drum Kit";
    for (size_t k = 0; k < 60; ++k) {
        inst.keyboard_map[k].note = static_cast<uint8_t>(k);
        inst.keyboard_map[k].sample = 1;
    }
    for (size_t k = 60; k < 120; ++k) {
        inst.keyboard_map[k].note = static_cast<uint8_t>(k);
        inst.keyboard_map[k].sample = 2;
    }

    tracker::Sample kick;
    kick.name = "Kick";
    kick.data8 = {50, 30, 0, -30, -50};
    kick.length = static_cast<uint32_t>(kick.data8.size());
    kick.volume = 64;

    tracker::Sample snare;
    snare.name = "Snare";
    snare.data16 = {10000, -8000, 4000, -2000};
    snare.length = static_cast<uint32_t>(snare.data16.size());
    snare.is_16bit = true;
    snare.volume = 55;

    inst.samples.push_back(std::move(kick));
    inst.samples.push_back(std::move(snare));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    const auto& li = loaded.instruments[0];
    REQUIRE_EQ(li.name, "Drum Kit");
    REQUIRE_EQ(li.samples.size(), 2);
    REQUIRE_EQ(li.samples[0].name, "Kick");
    REQUIRE(!li.samples[0].is_16bit);
    REQUIRE_EQ(li.samples[0].data8.size(), 5);
    REQUIRE_EQ(li.samples[1].name, "Snare");
    REQUIRE(li.samples[1].is_16bit);
    REQUIRE_EQ(li.samples[1].data16.size(), 4);
}

TEST_CASE(ItWriter_64Channels) {
    tracker::Song song;
    song.name = "64 Ch Song";
    song.num_channels = 64;
    song.order_table = {0};
    song.patterns.emplace_back(32, 64);

    for (size_t c = 0; c < 64; ++c) {
        song.channel_panning[c] = static_cast<uint8_t>(c % 64);
        song.channel_volume[c] = static_cast<uint8_t>(64 - (c % 32));
    }

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.num_channels, 64);
    for (size_t c = 0; c < 64; ++c) {
        REQUIRE_EQ(loaded.channel_volume[c], static_cast<uint8_t>(64 - (c % 32)));
    }
}

TEST_CASE(ItWriter_EmptyInstrument) {
    tracker::Song song;
    song.name = "Empty Inst";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Blank Inst";
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::it::ItWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::it::ItReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].name, "Blank Inst");
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 0);
}

