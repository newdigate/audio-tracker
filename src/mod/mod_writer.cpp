#include <tracker/mod/mod_writer.hpp>
#include <tracker/mod/mod_types.hpp>
#include <tracker/mod/mod_cell.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>

namespace tracker::mod {

Result<std::vector<uint8_t>> ModWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status ModWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status ModWriter::save(const Song& song, io::OutputStream& stream) {
    // 1. Song Title (20 bytes)
    stream.write_fixed_string(song.name, MOD_TITLE_LEN, '\0');

    // 2. 31 Sample Headers (30 bytes each)
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        if (s < song.instruments.size() && !song.instruments[s].samples.empty()) {
            const auto& sample = song.instruments[s].samples[0];
            stream.write_fixed_string(sample.name, MOD_SAMPLE_NAME_LEN, '\0');

            uint16_t len_words = static_cast<uint16_t>(sample.length / 2);
            stream.write_u16_be(len_words);

            uint8_t ft_nibble = static_cast<uint8_t>(sample.finetune & 0x0F);
            stream.write_u8(ft_nibble);
            stream.write_u8(std::min<uint8_t>(sample.volume, 64));

            if (sample.loop_type != LoopType::None && sample.loop_length > 0) {
                uint16_t lstart_words = static_cast<uint16_t>(sample.loop_start / 2);
                uint16_t llen_words = static_cast<uint16_t>(sample.loop_length / 2);
                stream.write_u16_be(lstart_words);
                stream.write_u16_be(llen_words);
            } else {
                stream.write_u16_be(0);
                stream.write_u16_be(1); // 1 word = standard ProTracker loop-off marker
            }
        } else {
            stream.write_zeros(MOD_SAMPLE_NAME_LEN);
            stream.write_u16_be(0); // length 0
            stream.write_u8(0);     // finetune 0
            stream.write_u8(0);     // volume 0
            stream.write_u16_be(0); // loop start 0
            stream.write_u16_be(1); // loop length 1
        }
    }

    // 3. Song Length & Restart Position
    uint8_t song_len = static_cast<uint8_t>(std::min<size_t>(song.order_table.size(), MOD_ORDER_TABLE_LEN));
    if (song_len == 0) song_len = 1;
    stream.write_u8(song_len);
    stream.write_u8(static_cast<uint8_t>(song.restart_position));

    // 4. 128-byte Order Table
    for (size_t i = 0; i < MOD_ORDER_TABLE_LEN; ++i) {
        if (i < song.order_table.size()) {
            stream.write_u8(song.order_table[i]);
        } else {
            stream.write_u8(0);
        }
    }

    // 5. Signature Tag (4 bytes)
    if (song.num_channels == 4) {
        stream.write("M.K.", 4);
    } else if (song.num_channels == 6) {
        stream.write("6CHN", 4);
    } else if (song.num_channels == 8) {
        stream.write("8CHN", 4);
    } else if (song.num_channels < 10) {
        std::string tag = std::to_string(song.num_channels) + "CHN";
        stream.write(tag.data(), 4);
    } else {
        std::string tag = std::to_string(song.num_channels) + "CN";
        stream.write(tag.data(), 4);
    }

    // 6. Pattern Data
    for (const auto& pat : song.patterns) {
        for (uint16_t row = 0; row < MOD_PATTERN_ROWS; ++row) {
            for (uint16_t ch = 0; ch < song.num_channels; ++ch) {
                uint8_t raw[4] = {0, 0, 0, 0};
                if (row < pat.num_rows && ch < pat.num_channels) {
                    pack_cell(pat.get_cell(row, ch), raw);
                }
                stream.write(raw, 4);
            }
        }
    }

    // 7. Sample Audio Data (Raw Signed 8-bit PCM)
    for (size_t s = 0; s < MOD_NUM_SAMPLES; ++s) {
        if (s < song.instruments.size() && !song.instruments[s].samples.empty()) {
            const auto& sample = song.instruments[s].samples[0];
            if (sample.is_16bit) {
                for (int16_t val : sample.data16) {
                    int8_t val8 = static_cast<int8_t>(val >> 8);
                    stream.write_i8(val8);
                }
            } else {
                if (!sample.data8.empty()) {
                    stream.write(sample.data8.data(), sample.data8.size());
                }
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::mod
