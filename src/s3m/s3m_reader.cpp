#include <tracker/s3m/s3m_reader.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>
#include <vector>
#include <array>

namespace tracker::s3m {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.length();
    while (end > 0 && (str[end - 1] == ' ' || str[end - 1] == '\t' || 
                       str[end - 1] == '\r' || str[end - 1] == '\n' || 
                       str[end - 1] == '\0')) {
        --end;
    }
    return str.substr(0, end);
}

Result<Song> S3mReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < S3M_HEADER_SIZE) {
        return Result<Song>(ErrorCode::InvalidSignature, "Data too small for S3M header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> S3mReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> S3mReader::load(io::InputStream& stream) {
    if (stream.size() > 0 && stream.size() < static_cast<int64_t>(S3M_HEADER_SIZE)) {
        return Result<Song>(ErrorCode::InvalidSignature, "Stream too small for S3M header");
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(28));

    uint8_t dos_eof = stream.read_u8();
    (void)dos_eof;
    uint8_t file_type = stream.read_u8();
    (void)file_type;
    uint16_t reserved1 = stream.read_u16_le();
    (void)reserved1;
    uint16_t ordnum = stream.read_u16_le();
    uint16_t insnum = stream.read_u16_le();
    uint16_t patnum = stream.read_u16_le();
    uint16_t flags = stream.read_u16_le();
    (void)flags;
    uint16_t cwt_vwt = stream.read_u16_le();
    uint16_t ffi = stream.read_u16_le();

    std::string magic = stream.read_fixed_string(4);
    if (magic != S3M_SIGNATURE_SONG) {
        return Result<Song>(ErrorCode::InvalidSignature, "Invalid S3M signature: " + magic);
    }

    song.tracker_name = "Scream Tracker";
    song.version = cwt_vwt;
    song.linear_frequency = false;

    uint8_t global_vol = stream.read_u8();
    song.global_volume = global_vol;

    uint8_t initial_speed = stream.read_u8();
    song.default_speed = (initial_speed > 0) ? initial_speed : 6;

    uint8_t initial_tempo = stream.read_u8();
    song.default_bpm = (initial_tempo > 0) ? initial_tempo : 125;

    uint8_t master_vol = stream.read_u8();
    song.mix_volume = master_vol & 0x7F;

    uint8_t ultraclick = stream.read_u8();
    (void)ultraclick;
    uint8_t default_pan_tag = stream.read_u8();

    stream.read_fixed_string(8); // reserved2 (8 bytes)

    uint16_t special_ptr = stream.read_u16_le();
    (void)special_ptr;

    std::array<uint8_t, 32> channel_settings{};
    stream.read(channel_settings.data(), 32);

    // Read Orders
    if (ordnum > 0) {
        song.order_table.resize(ordnum);
        stream.read(song.order_table.data(), ordnum);
    } else {
        song.order_table.clear();
    }

    // Read Sample Parapointers
    std::vector<uint16_t> ins_parapointers(insnum);
    for (uint16_t i = 0; i < insnum; ++i) {
        ins_parapointers[i] = stream.read_u16_le();
    }

    // Read Pattern Parapointers
    std::vector<uint16_t> pat_parapointers(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        pat_parapointers[p] = stream.read_u16_le();
    }

    // Read Custom Panning Table if present
    std::array<uint8_t, 32> custom_pan_table{};
    bool has_custom_pan = (default_pan_tag == S3M_DEFAULT_PANNING_TAG);
    if (has_custom_pan) {
        stream.read(custom_pan_table.data(), 32);
    }

    // Initialize Channel Panning & Volume
    for (size_t c = 0; c < 64; ++c) {
        song.channel_volume[c] = 64;
        if (c < 32) {
            if (has_custom_pan && (custom_pan_table[c] & 0x20)) {
                uint8_t pan_val = custom_pan_table[c] & 0x0F;
                song.channel_panning[c] = static_cast<uint8_t>(pan_val * 4);
            } else {
                uint8_t setting = channel_settings[c];
                if (setting != S3M_CHANNEL_DISABLED) {
                    uint8_t ch_type = setting & 0x0F;
                    song.channel_panning[c] = (ch_type < 8) ? 16 : 48;
                } else {
                    song.channel_panning[c] = 32;
                }
            }
        } else {
            song.channel_panning[c] = 32;
        }
    }

    // Read Samples
    song.instruments.resize(insnum);
    for (uint16_t s = 0; s < insnum; ++s) {
        if (ins_parapointers[s] == 0) {
            continue;
        }

        uint32_t smp_offset = parapointer_to_offset(ins_parapointers[s]);
        stream.seek(smp_offset, io::SeekOrigin::Begin);

        uint8_t sample_type = stream.read_u8();
        std::string dos_filename = trim_spaces(stream.read_fixed_string(12));
        uint8_t memseg_hi = stream.read_u8();
        uint16_t memseg_lo = stream.read_u16_le();
        uint32_t length = stream.read_u32_le();
        uint32_t loop_start = stream.read_u32_le();
        uint32_t loop_end = stream.read_u32_le();
        uint8_t volume = stream.read_u8();
        uint8_t dsk = stream.read_u8();
        (void)dsk;
        uint8_t pack = stream.read_u8();
        (void)pack;
        uint8_t flags_smp = stream.read_u8();
        uint32_t c5_speed = stream.read_u32_le();
        stream.read_fixed_string(12); // reserved
        std::string sample_name = trim_spaces(stream.read_fixed_string(28));
        std::string smp_magic = stream.read_fixed_string(4);

        if (smp_magic != S3M_SIGNATURE_SAMPLE) {
            continue;
        }

        Sample smp;
        smp.name = sample_name;
        smp.volume = std::min<uint8_t>(volume, 64);
        smp.c5_speed = (c5_speed > 0) ? c5_speed : 8363;
        smp.length = length;
        smp.is_16bit = ((flags_smp & S3M_SAMPLE_16BIT) != 0);

        if (flags_smp & S3M_SAMPLE_LOOP) {
            smp.loop_type = LoopType::Forward;
            smp.loop_start = loop_start;
            smp.loop_length = (loop_end > loop_start) ? (loop_end - loop_start) : 0;
        } else {
            smp.loop_type = LoopType::None;
            smp.loop_start = 0;
            smp.loop_length = 0;
        }

        uint32_t pcm_pp = (static_cast<uint32_t>(memseg_hi) << 16) | static_cast<uint32_t>(memseg_lo);
        uint32_t pcm_offset = parapointer_to_offset(pcm_pp);

        if (sample_type == S3M_SAMPLE_TYPE_PCM && length > 0 && pcm_offset > 0) {
            stream.seek(pcm_offset, io::SeekOrigin::Begin);
            if (smp.is_16bit) {
                smp.data16.resize(length);
                for (size_t f = 0; f < length; ++f) {
                    if (ffi == S3M_FFI_SIGNED) {
                        smp.data16[f] = stream.read_i16_le();
                    } else {
                        uint16_t u16 = stream.read_u16_le();
                        smp.data16[f] = static_cast<int16_t>(u16 ^ 0x8000);
                    }
                }
            } else {
                smp.data8.resize(length);
                for (size_t f = 0; f < length; ++f) {
                    if (ffi == S3M_FFI_SIGNED) {
                        smp.data8[f] = stream.read_i8();
                    } else {
                        uint8_t u8 = stream.read_u8();
                        smp.data8[f] = static_cast<int8_t>(u8 ^ 0x80);
                    }
                }
            }
        }

        Instrument inst;
        inst.name = smp.name;
        inst.filename = dos_filename;
        for (size_t k = 0; k < 120; ++k) {
            inst.keyboard_map[k].note = static_cast<uint8_t>(k);
            inst.keyboard_map[k].sample = static_cast<uint8_t>(s + 1);
        }
        inst.samples.push_back(std::move(smp));
        song.instruments[s] = std::move(inst);
    }

    // Determine active channels from channel settings and pattern usage
    uint16_t detected_channels = 0;
    for (size_t c = 0; c < 32; ++c) {
        if (channel_settings[c] != S3M_CHANNEL_DISABLED) {
            detected_channels = static_cast<uint16_t>(c + 1);
        }
    }

    // Read Patterns
    std::vector<Pattern> raw_patterns(patnum);
    uint16_t max_channel_used = 0;

    for (uint16_t p = 0; p < patnum; ++p) {
        if (pat_parapointers[p] == 0) {
            raw_patterns[p] = Pattern(static_cast<uint16_t>(S3M_ROWS_PER_PATTERN), static_cast<uint16_t>(S3M_MAX_CHANNELS));
            continue;
        }

        uint32_t pat_offset = parapointer_to_offset(pat_parapointers[p]);
        stream.seek(pat_offset, io::SeekOrigin::Begin);
        uint16_t packed_len = stream.read_u16_le();

        Pattern pat_32(static_cast<uint16_t>(S3M_ROWS_PER_PATTERN), static_cast<uint16_t>(S3M_MAX_CHANNELS));
        if (packed_len > 0) {
            auto unpack_res = unpack_pattern(stream, pat_32, static_cast<uint16_t>(S3M_MAX_CHANNELS));
            if (!unpack_res.is_ok()) return Result<Song>(unpack_res);
        }

        for (uint16_t r = 0; r < S3M_ROWS_PER_PATTERN; ++r) {
            for (uint16_t c = 0; c < S3M_MAX_CHANNELS; ++c) {
                if (!pat_32.get_cell(r, c).is_empty()) {
                    if (c + 1 > max_channel_used) {
                        max_channel_used = c + 1;
                    }
                }
            }
        }
        raw_patterns[p] = std::move(pat_32);
    }

    song.num_channels = std::max(detected_channels, max_channel_used);
    if (song.num_channels == 0) {
        song.num_channels = 4;
    }
    song.num_channels = std::min<uint16_t>(std::max<uint16_t>(song.num_channels, 1), static_cast<uint16_t>(S3M_MAX_CHANNELS));

    song.patterns.resize(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        const auto& pat_32 = raw_patterns[p];
        Pattern pat(pat_32.num_rows, song.num_channels);
        for (uint16_t r = 0; r < pat_32.num_rows; ++r) {
            for (uint16_t c = 0; c < song.num_channels; ++c) {
                pat.get_cell(r, c) = pat_32.get_cell(r, c);
            }
        }
        song.patterns[p] = std::move(pat);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::s3m
