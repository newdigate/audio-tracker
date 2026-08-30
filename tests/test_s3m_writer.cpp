#include "test_main.hpp"
#include <tracker/s3m/s3m_writer.hpp>
#include <tracker/s3m/s3m_reader.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE(S3mWriter_BasicSongExport) {
    tracker::Song song;
    song.name = "Test S3M Song";
    song.num_channels = 8;
    song.order_table = {0};
    song.patterns.emplace_back(64, 8);

    auto res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Test S3M Song");
    REQUIRE_EQ(load_res.value().num_channels, 8);
}

TEST_CASE(S3mWriter_SongMetadata) {
    tracker::Song song;
    song.name = "S3M Metadata";
    song.num_channels = 6;
    song.default_speed = 4;
    song.default_bpm = 140;
    song.global_volume = 55;
    song.mix_volume = 40;
    song.order_table = {0, 1, 0, 255};
    song.patterns.emplace_back(64, 6);
    song.patterns.emplace_back(64, 6);

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.name, "S3M Metadata");
    REQUIRE_EQ(loaded.num_channels, 6);
    REQUIRE_EQ(loaded.default_speed, 4);
    REQUIRE_EQ(loaded.default_bpm, 140);
    REQUIRE_EQ(loaded.global_volume, 55);
    REQUIRE_EQ(loaded.mix_volume, 40);
    REQUIRE_EQ(loaded.order_table.size(), 4);
    REQUIRE_EQ(loaded.order_table[0], 0);
    REQUIRE_EQ(loaded.order_table[1], 1);
    REQUIRE_EQ(loaded.order_table[2], 0);
    REQUIRE_EQ(loaded.order_table[3], 255);
    REQUIRE_EQ(loaded.patterns.size(), 2);
    REQUIRE_EQ(loaded.patterns[0].num_rows, 64);
    REQUIRE_EQ(loaded.patterns[1].num_rows, 64);
}

TEST_CASE(S3mWriter_Sample_8BitUnsigned) {
    tracker::Song song;
    song.name = "8Bit S3M Audio";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Bass Wave";
    inst.filename = "BASS.SMP";

    tracker::Sample smp;
    smp.name = "Bass Wave";
    smp.data8 = {0, 30, 60, 30, 0, -30, -60, -30};
    smp.length = static_cast<uint32_t>(smp.data8.size());
    smp.volume = 45;
    smp.c5_speed = 16726;
    smp.loop_type = tracker::LoopType::Forward;
    smp.loop_start = 2;
    smp.loop_length = 4;
    smp.is_16bit = false;

    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    const auto& ls = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls.name, "Bass Wave");
    REQUIRE(!ls.is_16bit);
    REQUIRE_EQ(ls.length, 8);
    REQUIRE_EQ(ls.volume, 45);
    REQUIRE_EQ(ls.c5_speed, 16726);
    REQUIRE(ls.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(ls.loop_start, 2);
    REQUIRE_EQ(ls.loop_length, 4);
    REQUIRE_EQ(ls.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(ls.data8[i], (std::vector<int8_t>{0, 30, 60, 30, 0, -30, -60, -30})[i]);
    }
}

TEST_CASE(S3mWriter_Sample_16Bit) {
    tracker::Song song;
    song.name = "16Bit S3M Audio";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "HiFi Lead";
    inst.filename = "LEAD.SMP";

    tracker::Sample smp;
    smp.name = "HiFi Lead";
    smp.data16 = {1000, 2000, -2000, -1000};
    smp.length = static_cast<uint32_t>(smp.data16.size());
    smp.volume = 60;
    smp.c5_speed = 44100;
    smp.loop_type = tracker::LoopType::Forward;
    smp.loop_start = 1;
    smp.loop_length = 2;
    smp.is_16bit = true;

    inst.samples.push_back(std::move(smp));
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 1);
    const auto& ls = loaded.instruments[0].samples[0];
    REQUIRE_EQ(ls.name, "HiFi Lead");
    REQUIRE(ls.is_16bit);
    REQUIRE_EQ(ls.length, 4);
    REQUIRE_EQ(ls.c5_speed, 44100);
    REQUIRE(ls.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(ls.loop_start, 1);
    REQUIRE_EQ(ls.loop_length, 2);
    REQUIRE_EQ(ls.data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(ls.data16[i], (std::vector<int16_t>{1000, 2000, -2000, -1000})[i]);
    }
}

TEST_CASE(S3mWriter_CustomPanning) {
    tracker::Song song;
    song.name = "Custom Pan S3M";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    song.channel_panning[0] = 0;   // Left (0 * 4 = 0)
    song.channel_panning[1] = 32;  // Center (8 * 4 = 32)
    song.channel_panning[2] = 60;  // Right (15 * 4 = 60)
    song.channel_panning[3] = 16;  // Mid-Left (4 * 4 = 16)

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.channel_panning[0], 0);
    REQUIRE_EQ(loaded.channel_panning[1], 32);
    REQUIRE_EQ(loaded.channel_panning[2], 60);
    REQUIRE_EQ(loaded.channel_panning[3], 16);
}

TEST_CASE(S3mWriter_PatternData) {
    tracker::Song song;
    song.name = "Pattern Test";
    song.num_channels = 4;
    song.order_table = {0};

    tracker::Pattern pat(64, 4);
    auto& c0 = pat.get_cell(0, 0);
    c0.note = 49; // C-4
    c0.instrument = 1;
    c0.volume = 48;
    c0.effect_type = static_cast<uint8_t>(tracker::s3m::S3mCommand::SetSpeed);
    c0.effect_param = 6;

    auto& c1 = pat.get_cell(5, 2);
    c1.note = 97; // Key off
    c1.instrument = 2;

    song.patterns.push_back(std::move(pat));

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.patterns.size(), 1);
    const auto& p = loaded.patterns[0];
    REQUIRE_EQ(p.num_rows, 64);
    REQUIRE_EQ(p.num_channels, 4);

    const auto& lc0 = p.get_cell(0, 0);
    REQUIRE_EQ(lc0.note, 49);
    REQUIRE_EQ(lc0.instrument, 1);
    REQUIRE_EQ(lc0.volume, 48);
    REQUIRE_EQ(lc0.effect_type, static_cast<uint8_t>(tracker::s3m::S3mCommand::SetSpeed));
    REQUIRE_EQ(lc0.effect_param, 6);

    const auto& lc1 = p.get_cell(5, 2);
    REQUIRE_EQ(lc1.note, 97);
    REQUIRE_EQ(lc1.instrument, 2);
}

TEST_CASE(S3mWriter_DirectStreamSave) {
    tracker::Song song;
    song.name = "Stream Save";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::io::MemoryOutputStream stream;
    auto status = tracker::s3m::S3mWriter::save(song, stream);
    REQUIRE(status.is_ok());
    REQUIRE(stream.data().size() > 0);

    auto load_res = tracker::s3m::S3mReader::load_from_memory(stream.data().data(), stream.data().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "Stream Save");
}

TEST_CASE(S3mWriter_SaveToFile_And_LoadFromFile) {
    tracker::Song song;
    song.name = "File Save Test";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    const std::string tmp_file = "test_s3m_writer_temp.s3m";
    auto save_status = tracker::s3m::S3mWriter::save_to_file(song, tmp_file);
    REQUIRE(save_status.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_file(tmp_file);
    std::remove(tmp_file.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File Save Test");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(S3mWriter_SaveToFile_Error) {
    tracker::Song song;
    auto status = tracker::s3m::S3mWriter::save_to_file(song, "/non_existent_dir_12345/test.s3m");
    REQUIRE(!status.is_ok());
    REQUIRE_EQ(status.code, tracker::ErrorCode::IoError);
}

TEST_CASE(S3mWriter_OrderTableClamping) {
    tracker::Song song;
    song.name = "Clamped Orders";
    song.num_channels = 4;
    song.order_table.resize(300);
    for (size_t i = 0; i < 300; ++i) {
        song.order_table[i] = static_cast<uint8_t>(i & 0xFF);
    }
    song.patterns.emplace_back(64, 4);

    auto res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(res.value().data(), res.value().size());
    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().order_table.size(), 256);
    for (size_t i = 0; i < 256; ++i) {
        REQUIRE_EQ(load_res.value().order_table[i], static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE(S3mWriter_32Channels) {
    tracker::Song song;
    song.name = "32 Ch S3M Song";
    song.num_channels = 32;
    song.order_table = {0};
    song.patterns.emplace_back(64, 32);

    for (size_t c = 0; c < 32; ++c) {
        song.channel_panning[c] = static_cast<uint8_t>((c % 16) * 4);
    }

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.num_channels, 32);
    for (size_t c = 0; c < 32; ++c) {
        REQUIRE_EQ(loaded.channel_panning[c], static_cast<uint8_t>((c % 16) * 4));
    }
}

TEST_CASE(S3mWriter_EmptyInstrument) {
    tracker::Song song;
    song.name = "Empty Inst";
    song.num_channels = 4;
    song.order_table = {0};
    song.patterns.emplace_back(64, 4);

    tracker::Instrument inst;
    inst.name = "Blank Inst";
    song.instruments.push_back(std::move(inst));

    auto write_res = tracker::s3m::S3mWriter::save_to_memory(song);
    REQUIRE(write_res.is_ok());

    auto load_res = tracker::s3m::S3mReader::load_from_memory(write_res.value().data(), write_res.value().size());
    REQUIRE(load_res.is_ok());

    const auto& loaded = load_res.value();
    REQUIRE_EQ(loaded.instruments.size(), 1);
    REQUIRE_EQ(loaded.instruments[0].samples.size(), 0);
}
