#include "test_main.hpp"
#include <tracker/model.hpp>

TEST_CASE(Model_CellAndPattern) {
    tracker::Cell cell;
    REQUIRE(cell.is_empty());
    REQUIRE_EQ(cell.note, 0);
    REQUIRE_EQ(cell.instrument, 0);
    REQUIRE_EQ(cell.volume, 0);
    REQUIRE_EQ(cell.effect_type, 0);
    REQUIRE_EQ(cell.effect_param, 0);

    cell.note = 12; // C-1
    REQUIRE(!cell.is_empty());
    cell.note = 0;
    cell.instrument = 1;
    REQUIRE(!cell.is_empty());
    cell.instrument = 0;
    cell.volume = 0x10;
    REQUIRE(!cell.is_empty());
    cell.volume = 0;
    cell.effect_type = 0x01;
    REQUIRE(!cell.is_empty());
    cell.effect_type = 0;
    cell.effect_param = 0x20;
    REQUIRE(!cell.is_empty());

    tracker::Pattern pat(64, 4);
    REQUIRE_EQ(pat.num_rows, 64);
    REQUIRE_EQ(pat.num_channels, 4);
    REQUIRE_EQ(pat.cells.size(), 64 * 4);
    REQUIRE(pat.is_all_empty());

    pat.get_cell(10, 2).note = 24;
    REQUIRE(!pat.is_all_empty());
    REQUIRE_EQ(pat.get_cell(10, 2).note, 24);

    const tracker::Pattern& const_pat = pat;
    REQUIRE_EQ(const_pat.get_cell(10, 2).note, 24);
    REQUIRE_EQ(const_pat.get_cell(0, 0).note, 0);
}

TEST_CASE(Model_SongSetup) {
    tracker::Song song;
    song.name = "My Song";
    song.num_channels = 8;
    song.patterns.emplace_back(64, 8);
    song.instruments.emplace_back();

    auto& inst = song.instruments.back();
    inst.name = "Lead Synth";
    inst.samples.emplace_back();
    inst.samples.back().name = "Sawtooth";
    inst.samples.back().volume = 64;

    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.instruments.size(), 1);
    REQUIRE_EQ(song.instruments[0].samples.size(), 1);
}

TEST_CASE(Model_SampleAndEnvelopeDefaults) {
    tracker::Sample sample;
    REQUIRE_EQ(sample.volume, 64);
    REQUIRE_EQ(sample.panning, 128);
    REQUIRE_EQ(sample.finetune, 0);
    REQUIRE_EQ(sample.relative_note, 0);
    REQUIRE(!sample.is_16bit);
    REQUIRE(sample.loop_type == tracker::LoopType::None);

    tracker::Envelope env;
    REQUIRE(!env.enabled);
    REQUIRE(!env.sustain_enabled);
    REQUIRE(!env.loop_enabled);
    REQUIRE_EQ(env.points.size(), 0);

    env.points.push_back({0, 64});
    env.points.push_back({10, 32});
    REQUIRE_EQ(env.points.size(), 2);
    REQUIRE_EQ(env.points[0].tick, 0);
    REQUIRE_EQ(env.points[0].value, 64);
    REQUIRE_EQ(env.points[1].tick, 10);
    REQUIRE_EQ(env.points[1].value, 32);

    tracker::Song song;
    REQUIRE_EQ(song.default_speed, 6);
    REQUIRE_EQ(song.default_bpm, 125);
    REQUIRE_EQ(song.version, 0x0104);
    REQUIRE(song.linear_frequency);
    REQUIRE_EQ(song.num_channels, 4);
}
