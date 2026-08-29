#include <tracker/xm/xm_reader.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <cstring>
#include <algorithm>

namespace tracker::xm {

static std::string trim_spaces(const std::string& str) {
    size_t end = str.length();
    while (end > 0 && (str[end - 1] == ' ' || str[end - 1] == '\t' || 
                       str[end - 1] == '\r' || str[end - 1] == '\n' || 
                       str[end - 1] == '\0')) {
        --end;
    }
    return str.substr(0, end);
}

Result<Song> XmReader::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < 60) {
        return Result<Song>(ErrorCode::InvalidSignature, "File too small for XM header");
    }
    io::MemoryInputStream stream(data, size);
    return load(stream);
}

Result<Song> XmReader::load_from_file(const std::string& path) {
    auto in_res = io::FileInputStream::open(path);
    if (!in_res.is_ok()) return Result<Song>(in_res.status());
    return load(in_res.value());
}

Result<Song> XmReader::load(io::InputStream& stream) {
    std::string sig = stream.read_fixed_string(XM_SIGNATURE_LEN);
    if (sig != XM_SIGNATURE) {
        return Result<Song>(ErrorCode::InvalidSignature, "Invalid XM signature: " + sig);
    }

    Song song;
    song.name = trim_spaces(stream.read_fixed_string(20));

    uint8_t eof_byte = stream.read_u8();
    if (eof_byte != XM_EOF_BYTE) {
        return Result<Song>(ErrorCode::CorruptHeader, "Invalid EOF byte in header");
    }

    song.tracker_name = trim_spaces(stream.read_fixed_string(20));
    song.version = stream.read_u16_le();

    int64_t header_size_pos = stream.tell();
    uint32_t header_size = stream.read_u32_le();
    int64_t pattern_start_offset = header_size_pos + header_size;

    uint16_t song_len = stream.read_u16_le();
    song.restart_position = stream.read_u16_le();
    song.num_channels = stream.read_u16_le();
    uint16_t num_patterns = stream.read_u16_le();
    uint16_t num_instruments = stream.read_u16_le();
    uint16_t flags = stream.read_u16_le();
    song.linear_frequency = ((flags & 1) != 0);
    song.default_speed = stream.read_u16_le();
    song.default_bpm = stream.read_u16_le();

    song.order_table.resize(256);
    for (size_t i = 0; i < 256; ++i) {
        song.order_table[i] = stream.read_u8();
    }
    if (song_len <= 256) {
        song.order_table.resize(song_len);
    }

    // Seek directly to start of patterns
    stream.seek(pattern_start_offset, io::SeekOrigin::Begin);

    // Read Patterns
    song.patterns.resize(num_patterns);
    for (uint16_t p = 0; p < num_patterns; ++p) {
        int64_t pat_header_start = stream.tell();
        uint32_t pat_header_len = stream.read_u32_le();
        uint8_t packing_type = stream.read_u8();
        (void)packing_type;
        uint16_t num_rows = stream.read_u16_le();
        uint16_t packed_size = stream.read_u16_le();

        Pattern pat(num_rows, song.num_channels);
        if (packed_size > 0) {
            std::vector<uint8_t> packed_data(packed_size);
            stream.read(packed_data.data(), packed_size);
            auto unpack_res = unpack_pattern(packed_data.data(), packed_size, pat);
            if (!unpack_res.is_ok()) return Result<Song>(unpack_res);
        }

        song.patterns[p] = std::move(pat);
        stream.seek(pat_header_start + pat_header_len + packed_size, io::SeekOrigin::Begin);
    }

    // Read Instruments
    song.instruments.resize(num_instruments);
    for (uint16_t inst_idx = 0; inst_idx < num_instruments; ++inst_idx) {
        int64_t inst_start = stream.tell();
        uint32_t inst_header_size = stream.read_u32_le();

        Instrument inst;
        inst.name = trim_spaces(stream.read_fixed_string(22));
        inst.type = stream.read_u8();
        uint16_t num_samples = stream.read_u16_le();

        struct RawSampleHeader {
            uint32_t length{0};
            uint32_t loop_start{0};
            uint32_t loop_length{0};
            uint8_t volume{64};
            int8_t finetune{0};
            uint8_t flags{0};
            uint8_t panning{128};
            int8_t relative_note{0};
            uint8_t reserved{0};
            std::string name;
        };

        std::vector<RawSampleHeader> raw_headers(num_samples);

        if (num_samples > 0) {
            uint32_t sample_header_size = stream.read_u32_le();
            for (size_t k = 0; k < 96; ++k) {
                inst.sample_map[k] = stream.read_u8();
            }

            // Volume Envelope Points (12 pairs of uint16_t)
            std::vector<EnvelopePoint> vol_points(12);
            for (size_t k = 0; k < 12; ++k) {
                vol_points[k].tick = stream.read_u16_le();
                vol_points[k].value = stream.read_u16_le();
            }

            // Panning Envelope Points (12 pairs of uint16_t)
            std::vector<EnvelopePoint> pan_points(12);
            for (size_t k = 0; k < 12; ++k) {
                pan_points[k].tick = stream.read_u16_le();
                pan_points[k].value = stream.read_u16_le();
            }

            uint8_t num_vol_pts = stream.read_u8();
            uint8_t num_pan_pts = stream.read_u8();
            inst.volume_envelope.sustain_point = stream.read_u8();
            inst.volume_envelope.loop_start_point = stream.read_u8();
            inst.volume_envelope.loop_end_point = stream.read_u8();
            inst.panning_envelope.sustain_point = stream.read_u8();
            inst.panning_envelope.loop_start_point = stream.read_u8();
            inst.panning_envelope.loop_end_point = stream.read_u8();

            uint8_t vol_type = stream.read_u8();
            inst.volume_envelope.enabled = ((vol_type & 1) != 0);
            inst.volume_envelope.sustain_enabled = ((vol_type & 2) != 0);
            inst.volume_envelope.loop_enabled = ((vol_type & 4) != 0);

            uint8_t pan_type = stream.read_u8();
            inst.panning_envelope.enabled = ((pan_type & 1) != 0);
            inst.panning_envelope.sustain_enabled = ((pan_type & 2) != 0);
            inst.panning_envelope.loop_enabled = ((pan_type & 4) != 0);

            inst.vibrato_type = stream.read_u8();
            inst.vibrato_sweep = stream.read_u8();
            inst.vibrato_depth = stream.read_u8();
            inst.vibrato_rate = stream.read_u8();
            inst.volume_fadeout = stream.read_u16_le();
            stream.read_u16_le(); // reserved

            vol_points.resize(std::min<size_t>(num_vol_pts, 12));
            pan_points.resize(std::min<size_t>(num_pan_pts, 12));
            inst.volume_envelope.points = std::move(vol_points);
            inst.panning_envelope.points = std::move(pan_points);

            // Seek to start of sample headers
            stream.seek(inst_start + inst_header_size, io::SeekOrigin::Begin);

            // Read Sample Headers
            for (uint16_t s = 0; s < num_samples; ++s) {
                int64_t sample_header_start = stream.tell();
                RawSampleHeader& rh = raw_headers[s];
                rh.length = stream.read_u32_le();
                rh.loop_start = stream.read_u32_le();
                rh.loop_length = stream.read_u32_le();
                rh.volume = stream.read_u8();
                rh.finetune = stream.read_i8();
                rh.flags = stream.read_u8();
                rh.panning = stream.read_u8();
                rh.relative_note = stream.read_i8();
                rh.reserved = stream.read_u8();
                rh.name = trim_spaces(stream.read_fixed_string(22));

                stream.seek(sample_header_start + sample_header_size, io::SeekOrigin::Begin);
            }

            // Read Sample Audio Data
            inst.samples.resize(num_samples);
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& rh = raw_headers[s];
                Sample sample;
                sample.name = rh.name;
                sample.volume = rh.volume;
                sample.finetune = rh.finetune;
                sample.panning = rh.panning;
                sample.relative_note = rh.relative_note;
                sample.is_16bit = ((rh.flags & XM_SAMPLE_16BIT) != 0);

                uint8_t loop_mode = (rh.flags & 3);
                if (loop_mode == XM_SAMPLE_LOOP_FORWARD) sample.loop_type = LoopType::Forward;
                else if (loop_mode == XM_SAMPLE_LOOP_PINGPONG) sample.loop_type = LoopType::PingPong;
                else sample.loop_type = LoopType::None;

                if (sample.is_16bit) {
                    uint32_t num_frames = rh.length / 2;
                    sample.length = num_frames;
                    sample.loop_start = rh.loop_start / 2;
                    sample.loop_length = rh.loop_length / 2;

                    if (rh.length > 0) {
                        std::vector<int16_t> delta_buf(num_frames);
                        for (size_t f = 0; f < num_frames; ++f) {
                            delta_buf[f] = stream.read_i16_le();
                        }
                        sample.data16 = decode_delta_16(delta_buf);
                    }
                } else {
                    sample.length = rh.length;
                    sample.loop_start = rh.loop_start;
                    sample.loop_length = rh.loop_length;

                    if (rh.length > 0) {
                        std::vector<int8_t> delta_buf(rh.length);
                        stream.read(delta_buf.data(), rh.length);
                        sample.data8 = decode_delta_8(delta_buf);
                    }
                }
                inst.samples[s] = std::move(sample);
            }
        } else {
            // Seek past empty instrument header
            stream.seek(inst_start + inst_header_size, io::SeekOrigin::Begin);
        }

        song.instruments[inst_idx] = std::move(inst);
    }

    return Result<Song>(std::move(song));
}

} // namespace tracker::xm
