#include "test_main.hpp"
#include <tracker/mod/mod_reader.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>

// Helper to construct a synthetic MOD module
struct SyntheticModBuilder {
    std::vector<uint8_t> buffer;

    SyntheticModBuilder() {
        buffer.assign(tracker::mod::MOD_HEADER_LEN, 0);
        // Default 4-channel "M.K."
        set_signature("M.K.");
        // Default song length 1, pattern 0
        set_song_length(1);
        set_order(0, 0);
    }

    void set_title(const std::string& title) {
        size_t len = std::min(title.size(), tracker::mod::MOD_TITLE_LEN);
        std::memset(buffer.data(), 0, tracker::mod::MOD_TITLE_LEN);
        std::memcpy(buffer.data(), title.data(), len);
    }

    void set_sample_header(size_t sample_idx, const std::string& name,
                           uint16_t len_words, int8_t finetune, uint8_t volume,
                           uint16_t loop_start_words, uint16_t loop_len_words) {
        if (sample_idx >= tracker::mod::MOD_NUM_SAMPLES) return;
        size_t offset = 20 + sample_idx * tracker::mod::MOD_SAMPLE_HEADER_LEN;

        // Name (22 bytes)
        size_t nlen = std::min(name.size(), tracker::mod::MOD_SAMPLE_NAME_LEN);
        std::memset(buffer.data() + offset, 0, tracker::mod::MOD_SAMPLE_NAME_LEN);
        std::memcpy(buffer.data() + offset, name.data(), nlen);

        // Length (2 bytes BE)
        buffer[offset + 22] = static_cast<uint8_t>((len_words >> 8) & 0xFF);
        buffer[offset + 23] = static_cast<uint8_t>(len_words & 0xFF);

        // Finetune (1 byte, lower 4 bits)
        uint8_t ft = static_cast<uint8_t>(finetune & 0x0F);
        buffer[offset + 24] = ft;

        // Volume (1 byte)
        buffer[offset + 25] = volume;

        // Loop start (2 bytes BE)
        buffer[offset + 26] = static_cast<uint8_t>((loop_start_words >> 8) & 0xFF);
        buffer[offset + 27] = static_cast<uint8_t>(loop_start_words & 0xFF);

        // Loop len (2 bytes BE)
        buffer[offset + 28] = static_cast<uint8_t>((loop_len_words >> 8) & 0xFF);
        buffer[offset + 29] = static_cast<uint8_t>(loop_len_words & 0xFF);
    }

    void set_song_length(uint8_t len) {
        buffer[950] = len;
    }

    void set_restart_position(uint8_t pos) {
        buffer[951] = pos;
    }

    void set_order(size_t index, uint8_t pattern_idx) {
        if (index < tracker::mod::MOD_ORDER_TABLE_LEN) {
            buffer[952 + index] = pattern_idx;
        }
    }

    void set_signature(const std::string& sig) {
        size_t len = std::min<size_t>(sig.size(), 4);
        for (size_t i = 0; i < 4; ++i) {
            buffer[1080 + i] = (i < len) ? static_cast<uint8_t>(sig[i]) : 0;
        }
    }

    void append_pattern(uint16_t num_channels, const std::vector<uint8_t>& raw_cells = {}) {
        size_t expected_size = tracker::mod::MOD_PATTERN_ROWS * num_channels * 4;
        if (raw_cells.empty()) {
            buffer.insert(buffer.end(), expected_size, 0);
        } else {
            buffer.insert(buffer.end(), raw_cells.begin(), raw_cells.end());
            if (raw_cells.size() < expected_size) {
                buffer.insert(buffer.end(), expected_size - raw_cells.size(), 0);
            }
        }
    }

    void append_sample_data(const std::vector<int8_t>& pcm_data) {
        for (int8_t b : pcm_data) {
            buffer.push_back(static_cast<uint8_t>(b));
        }
    }

    const uint8_t* data() const { return buffer.data(); }
    size_t size() const { return buffer.size(); }
};

TEST_CASE(ModReader_InvalidHeader) {
    std::vector<uint8_t> short_data(100, 0);
    auto res = tracker::mod::ModReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);

    auto res_null = tracker::mod::ModReader::load_from_memory(nullptr, 0);
    REQUIRE(!res_null.is_ok());
    REQUIRE_EQ(res_null.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(ModReader_InvalidSignature) {
    SyntheticModBuilder builder;
    builder.set_signature("XYZ!");
    builder.append_pattern(4);

    auto res = tracker::mod::ModReader::load_from_memory(builder.data(), builder.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(ModReader_Minimal4ChannelModule) {
    std::vector<uint8_t> mod_data(1084 + (64 * 4 * 4), 0);
    // Set song title
    std::memcpy(mod_data.data(), "Test MOD", 8);
    // Set song length
    mod_data[950] = 1;
    // Set pattern 0 in order table
    mod_data[952] = 0;
    // Set "M.K." signature at offset 1080
    mod_data[1080] = 'M';
    mod_data[1081] = '.';
    mod_data[1082] = 'K';
    mod_data[1083] = '.';

    // Cell at row 0, ch 0: C-1 on Sample 1
    // C-1 period = 856 = 0x0358, sample 1 = 0x01
    mod_data[1084] = 0x03;
    mod_data[1085] = 0x58;
    mod_data[1086] = 0x10;
    mod_data[1087] = 0x00;

    auto res = tracker::mod::ModReader::load_from_memory(mod_data.data(), mod_data.size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.name, "Test MOD");
    REQUIRE_EQ(song.num_channels, 4);
    REQUIRE(!song.linear_frequency);
    REQUIRE_EQ(song.default_speed, 6);
    REQUIRE_EQ(song.default_bpm, 125);
    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).note, 13);
    REQUIRE_EQ(song.patterns[0].get_cell(0, 0).instrument, 1);
    REQUIRE_EQ(song.instruments.size(), tracker::mod::MOD_NUM_SAMPLES);
}

TEST_CASE(ModReader_SignaturesAndChannelCounts) {
    auto test_sig = [](const std::string& sig, uint16_t expected_channels) {
        SyntheticModBuilder builder;
        builder.set_signature(sig);
        builder.append_pattern(expected_channels);

        auto res = tracker::mod::ModReader::load_from_memory(builder.data(), builder.size());
        REQUIRE(res.is_ok());
        REQUIRE_EQ(res.value().num_channels, expected_channels);
    };

    test_sig("M.K.", 4);
    test_sig("M!K!", 4);
    test_sig("4CHN", 4);
    test_sig("FLT4", 4);
    test_sig("6CHN", 6);
    test_sig("8CHN", 8);
    test_sig("OCTA", 8);
    test_sig("CD81", 8);
    test_sig("FLT8", 8);
    test_sig("16CN", 16);
    test_sig("32CN", 32);
    test_sig("12CH", 12);
    test_sig("28CH", 28);
    test_sig("5CHN", 5);
}

TEST_CASE(ModReader_SampleHeadersAndData) {
    SyntheticModBuilder builder;
    builder.set_title("Sample Test MOD");
    builder.set_signature("M.K.");

    // Sample 0: Lead (length 8 bytes = 4 words, finetune +2, vol 50, forward loop start=2, len=4)
    builder.set_sample_header(0, "Lead Synth", 4, 2, 50, 1, 2);
    // Sample 1: Bass (length 4 bytes = 2 words, finetune -3 (0x0D), vol 64, no loop)
    builder.set_sample_header(1, "Bass", 2, -3, 64, 0, 1);

    builder.append_pattern(4);

    std::vector<int8_t> pcm_lead = {10, 20, 30, 40, 50, 60, 70, 80};
    std::vector<int8_t> pcm_bass = {-10, -20, -30, -40};

    builder.append_sample_data(pcm_lead);
    builder.append_sample_data(pcm_bass);

    auto res = tracker::mod::ModReader::load_from_memory(builder.data(), builder.size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 31);

    // Instrument 0
    const auto& inst0 = song.instruments[0];
    REQUIRE_EQ(inst0.name, "Lead Synth");
    REQUIRE_EQ(inst0.samples.size(), 1);
    const auto& s0 = inst0.samples[0];
    REQUIRE_EQ(s0.name, "Lead Synth");
    REQUIRE_EQ(s0.length, 8);
    REQUIRE_EQ(s0.volume, 50);
    REQUIRE_EQ(s0.finetune, 2);
    REQUIRE(s0.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(s0.loop_start, 2);
    REQUIRE_EQ(s0.loop_length, 4);
    REQUIRE(!s0.is_16bit);
    REQUIRE_EQ(s0.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(s0.data8[i], pcm_lead[i]);
    }

    // Instrument 1
    const auto& inst1 = song.instruments[1];
    REQUIRE_EQ(inst1.name, "Bass");
    REQUIRE_EQ(inst1.samples.size(), 1);
    const auto& s1 = inst1.samples[0];
    REQUIRE_EQ(s1.name, "Bass");
    REQUIRE_EQ(s1.length, 4);
    REQUIRE_EQ(s1.volume, 64);
    REQUIRE_EQ(s1.finetune, -3);
    REQUIRE(s1.loop_type == tracker::LoopType::None);
    REQUIRE_EQ(s1.data8.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(s1.data8[i], pcm_bass[i]);
    }

    // Instrument 2 (empty)
    const auto& inst2 = song.instruments[2];
    REQUIRE_EQ(inst2.samples.size(), 0);
}

TEST_CASE(ModReader_MultiplePatternsAndOrderTable) {
    SyntheticModBuilder builder;
    builder.set_signature("M.K.");
    builder.set_song_length(4);
    builder.set_restart_position(2);
    builder.set_order(0, 0);
    builder.set_order(1, 2);
    builder.set_order(2, 1);
    builder.set_order(3, 2);

    // Max pattern index is 2, so 3 patterns total (0, 1, 2)
    builder.append_pattern(4); // Pattern 0
    builder.append_pattern(4); // Pattern 1
    builder.append_pattern(4); // Pattern 2

    auto res = tracker::mod::ModReader::load_from_memory(builder.data(), builder.size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.restart_position, 2);
    REQUIRE_EQ(song.order_table.size(), 4);
    REQUIRE_EQ(song.order_table[0], 0);
    REQUIRE_EQ(song.order_table[1], 2);
    REQUIRE_EQ(song.order_table[2], 1);
    REQUIRE_EQ(song.order_table[3], 2);
    REQUIRE_EQ(song.patterns.size(), 3);
    REQUIRE_EQ(song.patterns[0].num_rows, 64);
    REQUIRE_EQ(song.patterns[1].num_rows, 64);
    REQUIRE_EQ(song.patterns[2].num_rows, 64);
}

TEST_CASE(ModReader_CorruptPatternData) {
    SyntheticModBuilder builder;
    builder.set_signature("M.K.");
    // Truncate pattern data (less than 64 * 4 * 4 = 1024 bytes)
    builder.buffer.insert(builder.buffer.end(), 500, 0);

    auto res = tracker::mod::ModReader::load_from_memory(builder.data(), builder.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::CorruptPatternData);
}

TEST_CASE(ModReader_LoadFromFile) {
    SyntheticModBuilder builder;
    builder.set_title("File Test MOD");
    builder.set_signature("M.K.");
    builder.append_pattern(4);

    const std::string tmp_path = "test_temp_mod_reader.mod";
    {
        auto out_res = tracker::io::FileOutputStream::open(tmp_path);
        REQUIRE(out_res.is_ok());
        out_res.value().write(builder.data(), builder.size());
    }

    auto load_res = tracker::mod::ModReader::load_from_file(tmp_path);
    std::remove(tmp_path.c_str());

    REQUIRE(load_res.is_ok());
    REQUIRE_EQ(load_res.value().name, "File Test MOD");
    REQUIRE_EQ(load_res.value().num_channels, 4);
}

TEST_CASE(ModReader_FileNotFound) {
    auto res = tracker::mod::ModReader::load_from_file("non_existent_file_98765.mod");
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::IoError);
}
