#include <tracker/it/it_reader.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/it/it_compression.hpp>
#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>
#include <vector>
#include <array>
#include <cstring>

namespace tracker::it {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.length();
    while (end > 0 && (str[end - 1] == ' ' || str[end - 1] == '\t' || 
                       str[end - 1] == '\r' || str[end - 1] == '\n' || 
                       str[end - 1] == '\0')) {
        --end;
    }
    return str.substr(0, end);
}

Result<Song> ItReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < IT_HEADER_SIZE) {
        return Result<Song>(ErrorCode::InvalidSignature, "Data too small for IT header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> ItReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> ItReader::load(io::InputStream& stream) {
    if (stream.size() > 0 && stream.size() < static_cast<int64_t>(IT_HEADER_SIZE)) {
        return Result<Song>(ErrorCode::InvalidSignature, "Stream too small for IT header");
    }

    std::string magic = stream.read_fixed_string(4);
    if (magic != IT_SIGNATURE_SONG) {
        return Result<Song>(ErrorCode::InvalidSignature, "Invalid IT signature: " + magic);
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(26));

    uint16_t phighlight = stream.read_u16_le();
    (void)phighlight;
    uint16_t ordnum = stream.read_u16_le();
    uint16_t insnum = stream.read_u16_le();
    uint16_t smpnum = stream.read_u16_le();
    uint16_t patnum = stream.read_u16_le();
    uint16_t cwt_vwt = stream.read_u16_le();
    uint16_t cmwt = stream.read_u16_le();
    uint16_t flags = stream.read_u16_le();
    uint16_t special = stream.read_u16_le();

    song.tracker_name = "Impulse Tracker";
    song.version = cwt_vwt;
    song.linear_frequency = ((flags & IT_SONG_LINEAR_SLIDES) != 0);

    song.global_volume = stream.read_u8();
    song.mix_volume = stream.read_u8();
    song.default_speed = stream.read_u8();
    if (song.default_speed == 0) song.default_speed = 6;
    song.default_bpm = stream.read_u8();
    if (song.default_bpm == 0) song.default_bpm = 125;
    song.pan_separation = stream.read_u8();

    uint8_t pitch_wheel = stream.read_u8();
    (void)pitch_wheel;
    uint16_t msg_length = stream.read_u16_le();
    uint32_t msg_offset = stream.read_u32_le();
    uint32_t reserved = stream.read_u32_le();
    (void)reserved;

    std::array<uint8_t, 64> channel_pan{};
    stream.read(channel_pan.data(), 64);
    for (size_t c = 0; c < 64; ++c) {
        song.channel_panning[c] = channel_pan[c];
    }

    std::array<uint8_t, 64> channel_vol{};
    stream.read(channel_vol.data(), 64);
    for (size_t c = 0; c < 64; ++c) {
        song.channel_volume[c] = channel_vol[c];
    }

    // Read Orders
    if (ordnum > 0) {
        song.order_table.resize(ordnum);
        stream.read(song.order_table.data(), ordnum);
    } else {
        song.order_table.clear();
    }

    // Read Pointer Tables
    std::vector<uint32_t> inst_offsets(insnum);
    for (uint16_t i = 0; i < insnum; ++i) {
        inst_offsets[i] = stream.read_u32_le();
    }

    std::vector<uint32_t> smp_offsets(smpnum);
    for (uint16_t s = 0; s < smpnum; ++s) {
        smp_offsets[s] = stream.read_u32_le();
    }

    std::vector<uint32_t> pat_offsets(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        pat_offsets[p] = stream.read_u32_le();
    }

    // Read Song Message
    if ((special & IT_SPECIAL_MESSAGE) != 0 && msg_length > 0 && msg_offset > 0) {
        stream.seek(msg_offset, io::SeekOrigin::Begin);
        std::string msg(msg_length, '\0');
        stream.read(&msg[0], msg_length);
        while (!msg.empty() && msg.back() == '\0') {
            msg.pop_back();
        }
        song.message = std::move(msg);
    }

    // Read Samples
    std::vector<Sample> global_samples(smpnum);
    for (uint16_t s = 0; s < smpnum; ++s) {
        if (smp_offsets[s] == 0) {
            continue;
        }

        stream.seek(smp_offsets[s], io::SeekOrigin::Begin);
        std::string smp_magic = stream.read_fixed_string(4);
        if (smp_magic != IT_SIGNATURE_SAMPLE) {
            continue;
        }

        std::string dos_filename = trim_spaces(stream.read_fixed_string(12));
        (void)dos_filename;
        uint8_t smp_zero = stream.read_u8();
        (void)smp_zero;
        uint8_t global_vol = stream.read_u8();
        uint8_t smp_flags = stream.read_u8();
        uint8_t default_vol = stream.read_u8();
        std::string sample_name = trim_spaces(stream.read_fixed_string(26));
        uint8_t convert_flags = stream.read_u8();
        uint8_t default_pan = stream.read_u8();
        uint32_t length = stream.read_u32_le();
        uint32_t loop_start = stream.read_u32_le();
        uint32_t loop_end = stream.read_u32_le();
        uint32_t c5_speed = stream.read_u32_le();
        uint32_t sus_loop_start = stream.read_u32_le();
        uint32_t sus_loop_end = stream.read_u32_le();
        uint32_t sample_pointer = stream.read_u32_le();
        uint8_t vibrato_speed = stream.read_u8();
        uint8_t vibrato_depth = stream.read_u8();
        uint8_t vibrato_rate = stream.read_u8();
        uint8_t vibrato_wave = stream.read_u8();

        Sample smp;
        smp.name = sample_name;
        smp.global_volume = global_vol;
        smp.volume = default_vol;
        smp.c5_speed = (c5_speed > 0) ? c5_speed : 8363;
        smp.length = length;
        smp.loop_start = loop_start;
        smp.loop_length = (loop_end > loop_start) ? (loop_end - loop_start) : 0;
        smp.sustain_loop_start = sus_loop_start;
        smp.sustain_loop_length = (sus_loop_end > sus_loop_start) ? (sus_loop_end - sus_loop_start) : 0;

        if (smp_flags & IT_SAMPLE_LOOP) {
            smp.loop_type = (smp_flags & IT_SAMPLE_PINGPONG_LOOP) ? LoopType::PingPong : LoopType::Forward;
        } else {
            smp.loop_type = LoopType::None;
        }

        if (smp_flags & IT_SAMPLE_SUSTAIN_LOOP) {
            smp.sustain_loop_type = (smp_flags & IT_SAMPLE_PINGPONG_SUSTAIN) ? LoopType::PingPong : LoopType::Forward;
        } else {
            smp.sustain_loop_type = LoopType::None;
        }

        if (default_pan & 0x80) {
            uint8_t pan_val = default_pan & 0x7F;
            smp.panning = (pan_val >= 64) ? 255 : static_cast<uint8_t>(pan_val * 4);
        } else {
            smp.panning = 128;
        }

        smp.vibrato_sweep = vibrato_speed;
        smp.vibrato_depth = vibrato_depth;
        smp.vibrato_rate = vibrato_rate;
        smp.vibrato_type = vibrato_wave;

        // Read Audio Data
        if ((smp_flags & IT_SAMPLE_EXISTS) != 0 && length > 0 && sample_pointer > 0) {
            stream.seek(sample_pointer, io::SeekOrigin::Begin);
            bool is_16bit = ((smp_flags & IT_SAMPLE_16BIT) != 0);
            bool is_compressed = ((smp_flags & IT_SAMPLE_COMPRESSED) != 0);
            bool is_delta = ((convert_flags & IT_CONVERT_DELTA) != 0);
            bool is_signed = ((convert_flags & IT_CONVERT_SIGNED) != 0);
            bool is_it215 = (cwt_vwt >= 0x0215 || cmwt >= 0x0215);

            smp.is_16bit = is_16bit;

            if (is_compressed) {
                if (is_16bit) {
                    auto st = decompress_it_sample_16(stream, smp.data16, length, is_it215, is_delta);
                    if (!st.is_ok()) return Result<Song>(st);
                } else {
                    auto st = decompress_it_sample_8(stream, smp.data8, length, is_it215, is_delta);
                    if (!st.is_ok()) return Result<Song>(st);
                }
            } else {
                if (is_16bit) {
                    smp.data16.resize(length);
                    if (is_delta) {
                        int16_t last = 0;
                        for (size_t f = 0; f < length; ++f) {
                            int16_t d = stream.read_i16_le();
                            last += d;
                            smp.data16[f] = last;
                        }
                    } else {
                        for (size_t f = 0; f < length; ++f) {
                            if (is_signed) {
                                smp.data16[f] = stream.read_i16_le();
                            } else {
                                uint16_t u = stream.read_u16_le();
                                smp.data16[f] = static_cast<int16_t>(static_cast<int32_t>(u) - 32768);
                            }
                        }
                    }
                } else {
                    smp.data8.resize(length);
                    if (is_delta) {
                        int8_t last = 0;
                        for (size_t f = 0; f < length; ++f) {
                            int8_t d = stream.read_i8();
                            last += d;
                            smp.data8[f] = last;
                        }
                    } else {
                        for (size_t f = 0; f < length; ++f) {
                            if (is_signed) {
                                smp.data8[f] = stream.read_i8();
                            } else {
                                uint8_t u = stream.read_u8();
                                smp.data8[f] = static_cast<int8_t>(static_cast<int16_t>(u) - 128);
                            }
                        }
                    }
                }
            }
        }

        global_samples[s] = std::move(smp);
    }

    // Read Instruments
    bool use_instruments = (insnum > 0 && (flags & IT_SONG_INSTRUMENTS) != 0);
    if (use_instruments) {
        song.instruments.resize(insnum);
        for (uint16_t i = 0; i < insnum; ++i) {
            if (inst_offsets[i] == 0) {
                continue;
            }

            stream.seek(inst_offsets[i], io::SeekOrigin::Begin);
            std::string inst_magic = stream.read_fixed_string(4);
            if (inst_magic != IT_SIGNATURE_INST) {
                continue;
            }

            Instrument inst;
            inst.filename = trim_spaces(stream.read_fixed_string(12));
            uint8_t inst_zero = stream.read_u8();
            (void)inst_zero;
            uint8_t nna = stream.read_u8();
            inst.nna = static_cast<NewNoteAction>(nna & 3);
            uint8_t dct = stream.read_u8();
            inst.dct = static_cast<DuplicateCheckType>(dct & 3);
            uint8_t dca = stream.read_u8();
            inst.dca = static_cast<DuplicateCheckAction>(dca & 3);
            inst.volume_fadeout = stream.read_u16_le();
            uint8_t pitchpan_sep = stream.read_u8();
            (void)pitchpan_sep;
            uint8_t pitchpan_center = stream.read_u8();
            (void)pitchpan_center;
            inst.global_volume = stream.read_u8();
            uint8_t default_pan = stream.read_u8();
            if (default_pan & 0x80) {
                inst.default_panning = 128;
            } else {
                inst.default_panning = (default_pan >= 64) ? 255 : static_cast<uint8_t>(default_pan * 4);
            }
            uint16_t random_var = stream.read_u16_le();
            (void)random_var;
            uint16_t tracker_version = stream.read_u16_le();
            (void)tracker_version;
            uint8_t num_samples_inst = stream.read_u8();
            (void)num_samples_inst;
            uint8_t reserved_inst = stream.read_u8();
            (void)reserved_inst;
            inst.name = trim_spaces(stream.read_fixed_string(26));
            uint16_t initial_filter = stream.read_u16_le();
            (void)initial_filter;
            uint32_t midi_settings = stream.read_u32_le();
            (void)midi_settings;

            // Keyboard map (120 pairs)
            for (size_t k = 0; k < 120; ++k) {
                inst.keyboard_map[k].note = stream.read_u8();
                inst.keyboard_map[k].sample = stream.read_u8();
            }

            // Envelope reader helper (82 bytes)
            auto read_envelope = [](io::InputStream& s, Envelope& env) {
                uint8_t env_flags = s.read_u8();
                uint8_t num_nodes = s.read_u8();
                uint8_t loop_start = s.read_u8();
                uint8_t loop_end = s.read_u8();
                uint8_t sus_start = s.read_u8();
                uint8_t sus_end = s.read_u8();
                (void)sus_end;

                env.enabled = ((env_flags & 1) != 0);
                env.loop_enabled = ((env_flags & 2) != 0);
                env.sustain_enabled = ((env_flags & 4) != 0);
                env.loop_start_point = loop_start;
                env.loop_end_point = loop_end;
                env.sustain_point = sus_start;

                uint8_t valid_nodes = std::min<uint8_t>(num_nodes, 25);
                env.points.resize(valid_nodes);
                for (uint8_t n = 0; n < 25; ++n) {
                    uint8_t val = s.read_u8();
                    uint16_t tick = s.read_u16_le();
                    if (n < valid_nodes) {
                        env.points[n].value = val;
                        env.points[n].tick = tick;
                    }
                }
                s.read_u8(); // padding to 82 bytes
            };

            read_envelope(stream, inst.volume_envelope);
            read_envelope(stream, inst.panning_envelope);
            read_envelope(stream, inst.pitch_envelope);

            stream.read_u32_le(); // 4 reserved bytes

            // Assign samples associated with this instrument
            std::vector<uint8_t> used_samples;
            for (size_t k = 0; k < 120; ++k) {
                uint8_t s_idx = inst.keyboard_map[k].sample;
                if (s_idx > 0 && s_idx <= global_samples.size()) {
                    if (std::find(used_samples.begin(), used_samples.end(), s_idx) == used_samples.end()) {
                        used_samples.push_back(s_idx);
                    }
                }
            }

            for (uint8_t s_idx : used_samples) {
                inst.samples.push_back(global_samples[s_idx - 1]);
            }
            if (inst.samples.empty() && i < global_samples.size()) {
                inst.samples.push_back(global_samples[i]);
            }

            song.instruments[i] = std::move(inst);
        }
    } else {
        // Sample Mode
        song.instruments.resize(smpnum);
        for (uint16_t s = 0; s < smpnum; ++s) {
            Instrument inst;
            inst.name = global_samples[s].name;
            for (size_t k = 0; k < 120; ++k) {
                inst.keyboard_map[k].note = static_cast<uint8_t>(k);
                inst.keyboard_map[k].sample = static_cast<uint8_t>(s + 1);
            }
            inst.samples.push_back(std::move(global_samples[s]));
            song.instruments[s] = std::move(inst);
        }
    }

    // Determine active channels from channel panning table
    uint16_t detected_channels = 0;
    for (size_t c = 0; c < 64; ++c) {
        if (song.channel_panning[c] < 128) {
            detected_channels = static_cast<uint16_t>(c + 1);
        }
    }
    if (detected_channels == 0) {
        detected_channels = 4;
    }

    // Read Patterns
    std::vector<Pattern> raw_patterns(patnum);
    uint16_t max_channel_used = 0;

    for (uint16_t p = 0; p < patnum; ++p) {
        if (pat_offsets[p] == 0) {
            raw_patterns[p] = Pattern(64, 64);
            continue;
        }

        stream.seek(pat_offsets[p], io::SeekOrigin::Begin);
        uint16_t pat_len = stream.read_u16_le();
        uint16_t num_rows = stream.read_u16_le();
        stream.read_u32_le(); // 4 reserved bytes
        if (num_rows == 0) num_rows = 64;

        Pattern pat_64(num_rows, 64);
        if (pat_len > 0) {
            auto unpack_res = unpack_pattern(stream, pat_64, num_rows, 64);
            if (!unpack_res.is_ok()) return Result<Song>(unpack_res);
        }

        for (uint16_t r = 0; r < num_rows; ++r) {
            for (uint16_t c = 0; c < 64; ++c) {
                if (!pat_64.get_cell(r, c).is_empty()) {
                    if (c + 1 > max_channel_used) {
                        max_channel_used = c + 1;
                    }
                }
            }
        }
        raw_patterns[p] = std::move(pat_64);
    }

    song.num_channels = std::max(detected_channels, max_channel_used);
    song.num_channels = std::min<uint16_t>(std::max<uint16_t>(song.num_channels, 1), 64);

    song.patterns.resize(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        const auto& pat_64 = raw_patterns[p];
        Pattern pat(pat_64.num_rows, song.num_channels);
        for (uint16_t r = 0; r < pat_64.num_rows; ++r) {
            for (uint16_t c = 0; c < song.num_channels; ++c) {
                pat.get_cell(r, c) = pat_64.get_cell(r, c);
            }
        }
        song.patterns[p] = std::move(pat);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::it
