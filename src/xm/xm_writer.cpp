#include <tracker/xm/xm_writer.hpp>
#include <tracker/xm/xm_types.hpp>
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>

namespace tracker::xm {

Result<std::vector<uint8_t>> XmWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status XmWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status XmWriter::save(const Song& song, io::OutputStream& stream) {
    // 1. Module Header
    stream.write(XM_SIGNATURE, XM_SIGNATURE_LEN);
    stream.write_fixed_string(song.name, 20, ' ');
    stream.write_u8(XM_EOF_BYTE);
    stream.write_fixed_string(song.tracker_name.empty() ? "FastTracker v2.00   " : song.tracker_name, 20, ' ');
    stream.write_u16_le(song.version != 0 ? song.version : XM_VERSION_104);
    stream.write_u32_le(XM_HEADER_SIZE_104);

    uint16_t song_len = static_cast<uint16_t>(std::min<size_t>(song.order_table.size(), 256));
    stream.write_u16_le(song_len);
    stream.write_u16_le(song.restart_position);
    stream.write_u16_le(song.num_channels);
    stream.write_u16_le(static_cast<uint16_t>(song.patterns.size()));
    stream.write_u16_le(static_cast<uint16_t>(song.instruments.size()));

    uint16_t flags = song.linear_frequency ? 1 : 0;
    stream.write_u16_le(flags);
    stream.write_u16_le(song.default_speed);
    stream.write_u16_le(song.default_bpm);

    // 256-byte Order Table
    for (size_t i = 0; i < 256; ++i) {
        if (i < song.order_table.size()) {
            stream.write_u8(song.order_table[i]);
        } else {
            stream.write_u8(0);
        }
    }

    // 2. Patterns
    for (const auto& pat : song.patterns) {
        auto packed = pack_pattern(pat);
        stream.write_u32_le(XM_PATTERN_HEADER_LEN);
        stream.write_u8(0); // packing type
        stream.write_u16_le(pat.num_rows);
        stream.write_u16_le(static_cast<uint16_t>(packed.size()));
        if (!packed.empty()) {
            stream.write(packed.data(), packed.size());
        }
    }

    // 3. Instruments
    for (const auto& inst : song.instruments) {
        uint16_t num_samples = static_cast<uint16_t>(std::min<size_t>(inst.samples.size(), 16));
        if (num_samples == 0) {
            // Write 29-byte empty instrument header
            stream.write_u32_le(XM_INST_HEADER_EMPTY_LEN);
            stream.write_fixed_string(inst.name, 22, ' ');
            stream.write_u8(inst.type);
            stream.write_u16_le(0);
        } else {
            // Write 263-byte full instrument header
            stream.write_u32_le(XM_INST_HEADER_FULL_LEN);
            stream.write_fixed_string(inst.name, 22, ' ');
            stream.write_u8(inst.type);
            stream.write_u16_le(num_samples);
            stream.write_u32_le(XM_SAMPLE_HEADER_LEN);

            for (size_t k = 0; k < 96; ++k) {
                stream.write_u8(inst.sample_map[k]);
            }

            // Write 12 Volume Envelope Points (24 words)
            for (size_t k = 0; k < 12; ++k) {
                if (k < inst.volume_envelope.points.size()) {
                    stream.write_u16_le(inst.volume_envelope.points[k].tick);
                    stream.write_u16_le(inst.volume_envelope.points[k].value);
                } else {
                    stream.write_u16_le(0);
                    stream.write_u16_le(0);
                }
            }

            // Write 12 Panning Envelope Points (24 words)
            for (size_t k = 0; k < 12; ++k) {
                if (k < inst.panning_envelope.points.size()) {
                    stream.write_u16_le(inst.panning_envelope.points[k].tick);
                    stream.write_u16_le(inst.panning_envelope.points[k].value);
                } else {
                    stream.write_u16_le(0);
                    stream.write_u16_le(0);
                }
            }

            stream.write_u8(static_cast<uint8_t>(std::min<size_t>(inst.volume_envelope.points.size(), 12)));
            stream.write_u8(static_cast<uint8_t>(std::min<size_t>(inst.panning_envelope.points.size(), 12)));
            stream.write_u8(inst.volume_envelope.sustain_point);
            stream.write_u8(inst.volume_envelope.loop_start_point);
            stream.write_u8(inst.volume_envelope.loop_end_point);
            stream.write_u8(inst.panning_envelope.sustain_point);
            stream.write_u8(inst.panning_envelope.loop_start_point);
            stream.write_u8(inst.panning_envelope.loop_end_point);

            uint8_t vol_type = (inst.volume_envelope.enabled ? 1 : 0) |
                               (inst.volume_envelope.sustain_enabled ? 2 : 0) |
                               (inst.volume_envelope.loop_enabled ? 4 : 0);
            stream.write_u8(vol_type);

            uint8_t pan_type = (inst.panning_envelope.enabled ? 1 : 0) |
                               (inst.panning_envelope.sustain_enabled ? 2 : 0) |
                               (inst.panning_envelope.loop_enabled ? 4 : 0);
            stream.write_u8(pan_type);

            stream.write_u8(inst.vibrato_type);
            stream.write_u8(inst.vibrato_sweep);
            stream.write_u8(inst.vibrato_depth);
            stream.write_u8(inst.vibrato_rate);
            stream.write_u16_le(inst.volume_fadeout);
            stream.write_zeros(22); // reserved (22 bytes to complete 263-byte header)

            // Sample Headers
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& sample = inst.samples[s];
                uint32_t byte_len = sample.is_16bit ? (sample.length * 2) : sample.length;
                uint32_t loop_start_bytes = sample.is_16bit ? (sample.loop_start * 2) : sample.loop_start;
                uint32_t loop_len_bytes = sample.is_16bit ? (sample.loop_length * 2) : sample.loop_length;

                stream.write_u32_le(byte_len);
                stream.write_u32_le(loop_start_bytes);
                stream.write_u32_le(loop_len_bytes);
                stream.write_u8(sample.volume);
                stream.write_i8(sample.finetune);

                uint8_t loop_mode = 0;
                if (sample.loop_type == LoopType::Forward) loop_mode = XM_SAMPLE_LOOP_FORWARD;
                else if (sample.loop_type == LoopType::PingPong) loop_mode = XM_SAMPLE_LOOP_PINGPONG;

                uint8_t flags_byte = loop_mode | (sample.is_16bit ? XM_SAMPLE_16BIT : 0);
                stream.write_u8(flags_byte);
                stream.write_u8(sample.panning);
                stream.write_i8(sample.relative_note);
                stream.write_u8(0); // reserved
                stream.write_fixed_string(sample.name, 22, ' ');
            }

            // Sample Audio Data (Delta Encoded)
            for (uint16_t s = 0; s < num_samples; ++s) {
                const auto& sample = inst.samples[s];
                if (sample.is_16bit) {
                    if (!sample.data16.empty()) {
                        auto delta = encode_delta_16(sample.data16);
                        for (int16_t val : delta) {
                            stream.write_i16_le(val);
                        }
                    }
                } else {
                    if (!sample.data8.empty()) {
                        auto delta = encode_delta_8(sample.data8);
                        stream.write(delta.data(), delta.size());
                    }
                }
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::xm
