#include "test_main.hpp"
#include <tracker/model.hpp>

TEST_CASE(Model_ItExtensions) {
    tracker::Sample sample;
    REQUIRE_EQ(sample.global_volume, 64);
    REQUIRE_EQ(sample.c5_speed, 8363);
    REQUIRE(sample.sustain_loop_type == tracker::LoopType::None);
    REQUIRE_EQ(sample.sustain_loop_start, 0);
    REQUIRE_EQ(sample.sustain_loop_length, 0);

    tracker::Instrument inst;
    REQUIRE(inst.nna == tracker::NewNoteAction::Cut);
    REQUIRE(inst.dct == tracker::DuplicateCheckType::Off);
    REQUIRE(inst.dca == tracker::DuplicateCheckAction::Cut);
    REQUIRE_EQ(inst.global_volume, 128);
    REQUIRE_EQ(inst.default_panning, 128);
    REQUIRE_EQ(inst.keyboard_map.size(), 120);

    // Initial keyboard mapping defaults: note matches index, sample 0
    for (size_t i = 0; i < 120; ++i) {
        REQUIRE_EQ(inst.keyboard_map[i].note, static_cast<uint8_t>(i));
        REQUIRE_EQ(inst.keyboard_map[i].sample, 0);
    }

    tracker::Song song;
    REQUIRE_EQ(song.num_channels, 4); // Default 4 channels
    REQUIRE_EQ(song.global_volume, 128);
    REQUIRE_EQ(song.mix_volume, 48);
    REQUIRE_EQ(song.pan_separation, 128);
    REQUIRE_EQ(song.channel_volume.size(), 64);
    REQUIRE_EQ(song.channel_panning.size(), 64);
}
