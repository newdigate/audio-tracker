#include <tracker/mod/mod_reader.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/mod/mod_cell.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstring>
#include <algorithm>

namespace tracker::mod {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.length();
    while (end > 0 && (str[end - 1] == ' ' || str[end - 1] == '\t' || 
                       str[end - 1] == '\r' || str[end - 1] == '\n' || 
                       str[end - 1] == '\0')) {
        --end;
    }
    return str.substr(0, end);
}

static uint16_t parse_channel_count(const std::string& tag) {
    if (tag == "M.K." || tag == "M!K!" || tag == "4CHN" || tag == "FLT4") return 4;
    if (tag == "6CHN") return 6;
    if (tag == "8CHN" || tag == "OCTA" || tag == "CD81" || tag == "FLT8") return 8;
    if (tag == "16CN") return 16;
    if (tag == "32CN") return 32;

    if (tag.size() == 4) {
        if (tag[2] == 'C' && (tag[3] == 'H' || tag[3] == 'N')) {
            int ch = (tag[0] - '0') * 10 + (tag[1] - '0');
            if (ch > 0 && ch <= 32) return static_cast<uint16_t>(ch);
        } else if (tag[0] >= '1' && tag[0] <= '9' && tag.substr(1) == "CHN") {
            return static_cast<uint16_t>(tag[0] - '0');
        }
    }
    return 0;
}

Result<Song> ModReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < MOD_HEADER_LEN) {
        return Result<Song>(ErrorCode::InvalidSignature, "Data too small for MOD header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> ModReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> ModReader::load(io::InputStream& stream) {
    if (stream.size() > 0 && stream.size() < static_cast<int64_t>(MOD_HEADER_LEN)) {
        return Result<Song>(ErrorCode::InvalidSignature, "Stream too small for MOD header");
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(MOD_TITLE_LEN));

    struct RawSampleInfo {
        std::string name;
        uint32_t length_bytes{0};
        int8_t finetune{0};
        uint8_t volume{64};
        uint32_t loop_start_bytes{0};
        uint32_t loop_length_bytes{0};
    };

    std::vector<RawSampleInfo> raw_samples(MOD_NUM_SAMPLES);
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        RawSampleInfo& info = raw_samples[s];
        info.name = trim_spaces(stream.read_fixed_string(MOD_SAMPLE_NAME_LEN));
        uint16_t len_words = stream.read_u16_be();
        info.length_bytes = static_cast<uint32_t>(len_words) * 2;

        uint8_t ft = stream.read_u8() & 0x0F;
        info.finetune = (ft >= 8) ? static_cast<int8_t>(ft - 16) : static_cast<int8_t>(ft);
        info.volume = std::min<uint8_t>(stream.read_u8(), 64);

        uint16_t lstart_words = stream.read_u16_be();
        info.loop_start_bytes = static_cast<uint32_t>(lstart_words) * 2;
        uint16_t llen_words = stream.read_u16_be();
        info.loop_length_bytes = static_cast<uint32_t>(llen_words) * 2;
    }

    uint8_t song_len = stream.read_u8();
    uint8_t restart_pos = stream.read_u8();
    song.restart_position = restart_pos;

    std::vector<uint8_t> raw_orders(MOD_ORDER_TABLE_LEN);
    stream.read(raw_orders.data(), MOD_ORDER_TABLE_LEN);

    uint8_t effective_len = std::min<uint8_t>(song_len, 128);
    if (effective_len == 0) effective_len = 1;
    song.order_table.assign(raw_orders.begin(), raw_orders.begin() + effective_len);

    std::string tag = stream.read_fixed_string(4);
    uint16_t num_channels = parse_channel_count(tag);
    if (num_channels == 0) {
        return Result<Song>(ErrorCode::InvalidSignature, "Unknown MOD signature: " + tag);
    }
    song.num_channels = num_channels;
    song.linear_frequency = false;
    song.default_speed = 6;
    song.default_bpm = 125;

    // Find highest pattern index in order table
    uint8_t max_pat_idx = 0;
    for (uint8_t p : song.order_table) {
        if (p > max_pat_idx) max_pat_idx = p;
    }
    uint16_t num_patterns = static_cast<uint16_t>(max_pat_idx + 1);

    // Read Patterns
    song.patterns.resize(num_patterns);
    for (uint16_t p = 0; p < num_patterns; ++p) {
        Pattern pat(MOD_PATTERN_ROWS, song.num_channels);
        for (uint16_t row = 0; row < MOD_PATTERN_ROWS; ++row) {
            for (uint16_t ch = 0; ch < song.num_channels; ++ch) {
                uint8_t cell_bytes[4] = {0, 0, 0, 0};
                if (stream.read(cell_bytes, 4) < 4) {
                    return Result<Song>(ErrorCode::CorruptPatternData, "Truncated pattern data");
                }
                unpack_cell(cell_bytes, pat.get_cell(row, ch));
            }
        }
        song.patterns[p] = std::move(pat);
    }

    // Read Sample Data
    song.instruments.resize(MOD_NUM_SAMPLES);
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        const auto& raw_info = raw_samples[s];
        Instrument inst;
        inst.name = raw_info.name;

        if (raw_info.length_bytes > 0) {
            Sample sample;
            sample.name = raw_info.name;
            sample.length = raw_info.length_bytes;
            sample.volume = raw_info.volume;
            sample.finetune = raw_info.finetune;
            sample.is_16bit = false;

            if (raw_info.loop_length_bytes > 2) {
                sample.loop_type = LoopType::Forward;
                sample.loop_start = raw_info.loop_start_bytes;
                sample.loop_length = raw_info.loop_length_bytes;
            } else {
                sample.loop_type = LoopType::None;
                sample.loop_start = 0;
                sample.loop_length = 0;
            }

            sample.data8.resize(raw_info.length_bytes);
            if (stream.read(sample.data8.data(), raw_info.length_bytes) < raw_info.length_bytes) {
                // If stream terminates early, preserve whatever was read
            }
            inst.samples.push_back(std::move(sample));
        }
        song.instruments[s] = std::move(inst);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::mod
