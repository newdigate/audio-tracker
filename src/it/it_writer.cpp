#include <tracker/it/it_writer.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>
#include <vector>
#include <array>
#include <cstring>

namespace tracker::it {

Result<std::vector<uint8_t>> ItWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status ItWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status ItWriter::save(const Song& song, io::OutputStream& stream) {
    uint16_t num_channels = std::min<uint16_t>(std::max<uint16_t>(song.num_channels, 1), 64);
    uint16_t ordnum = static_cast<uint16_t>(std::min<size_t>(song.order_table.size(), IT_MAX_ORDERS));
    uint16_t insnum = static_cast<uint16_t>(std::min<size_t>(song.instruments.size(), IT_MAX_INSTRUMENTS));
    uint16_t patnum = static_cast<uint16_t>(std::min<size_t>(song.patterns.size(), IT_MAX_PATTERNS));

    // 1. Collect global samples from instruments
    std::vector<const Sample*> global_samples;
    std::vector<size_t> sample_offset_for_inst(insnum, 0);

    for (uint16_t i = 0; i < insnum; ++i) {
        sample_offset_for_inst[i] = global_samples.size();
        const auto& inst = song.instruments[i];
        for (const auto& smp : inst.samples) {
            if (global_samples.size() < IT_MAX_SAMPLES) {
                global_samples.push_back(&smp);
            }
        }
    }
    uint16_t smpnum = static_cast<uint16_t>(global_samples.size());

    // 2. Pre-pack patterns
    std::vector<std::vector<uint8_t>> packed_patterns(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        const auto& pat = song.patterns[p];
        io::MemoryOutputStream pat_stream;
        auto pack_res = pack_pattern(pat, pat_stream);
        if (!pack_res.is_ok()) return pack_res;
        packed_patterns[p] = pat_stream.take_data();
    }

    // 3. Compute file offsets
    uint32_t current_offset = IT_HEADER_SIZE + ordnum + static_cast<uint32_t>((insnum + smpnum + patnum) * 4);

    bool has_msg = !song.message.empty();
    uint16_t msg_length = 0;
    uint32_t msg_offset = 0;
    if (has_msg) {
        msg_length = static_cast<uint16_t>(song.message.size() + 1);
        msg_offset = current_offset;
        current_offset += msg_length;
    }

    std::vector<uint32_t> inst_offsets(insnum, 0);
    for (uint16_t i = 0; i < insnum; ++i) {
        inst_offsets[i] = current_offset;
        current_offset += IT_INST_HEADER_SIZE;
    }

    std::vector<uint32_t> smp_offsets(smpnum, 0);
    for (uint16_t s = 0; s < smpnum; ++s) {
        smp_offsets[s] = current_offset;
        current_offset += IT_SAMPLE_HEADER_SIZE;
    }

    std::vector<uint32_t> pat_offsets(patnum, 0);
    for (uint16_t p = 0; p < patnum; ++p) {
        pat_offsets[p] = current_offset;
        current_offset += 8 + static_cast<uint32_t>(packed_patterns[p].size());
    }

    std::vector<uint32_t> smp_pcm_offsets(smpnum, 0);
    for (uint16_t s = 0; s < smpnum; ++s) {
        const auto& smp = *global_samples[s];
        uint32_t pcm_bytes = 0;
        if (smp.is_16bit) {
            pcm_bytes = static_cast<uint32_t>(smp.data16.size() * 2);
        } else {
            pcm_bytes = static_cast<uint32_t>(smp.data8.size());
        }
        if (pcm_bytes > 0) {
            smp_pcm_offsets[s] = current_offset;
            current_offset += pcm_bytes;
        } else {
            smp_pcm_offsets[s] = 0;
        }
    }

    // 4. Write IT Header (192 bytes)
    stream.write(IT_SIGNATURE_SONG, IT_SIGNATURE_SONG_LEN);
    stream.write_fixed_string(song.name, 26, '\0');
    stream.write_u16_le(0); // phighlight
    stream.write_u16_le(ordnum);
    stream.write_u16_le(insnum);
    stream.write_u16_le(smpnum);
    stream.write_u16_le(patnum);
    stream.write_u16_le(song.version != 0 ? song.version : 0x0214);
    stream.write_u16_le(0x0200); // cmwt

    uint16_t flags = IT_SONG_STEREO;
    if (insnum > 0) flags |= IT_SONG_INSTRUMENTS;
    if (song.linear_frequency) flags |= IT_SONG_LINEAR_SLIDES;
    stream.write_u16_le(flags);

    uint16_t special = has_msg ? IT_SPECIAL_MESSAGE : 0;
    stream.write_u16_le(special);

    stream.write_u8(song.global_volume != 0 ? song.global_volume : 128);
    stream.write_u8(song.mix_volume != 0 ? song.mix_volume : 48);
    stream.write_u8(song.default_speed != 0 ? static_cast<uint8_t>(song.default_speed) : 6);
    stream.write_u8(song.default_bpm != 0 ? static_cast<uint8_t>(song.default_bpm) : 125);
    stream.write_u8(song.pan_separation != 0 ? song.pan_separation : 128);
    stream.write_u8(0); // pitch_wheel

    stream.write_u16_le(msg_length);
    stream.write_u32_le(msg_offset);
    stream.write_u32_le(0); // reserved

    // Channel panning (64 bytes)
    bool has_custom_panning = false;
    for (size_t c = 0; c < num_channels; ++c) {
        if (song.channel_panning[c] != 0) {
            has_custom_panning = true;
            break;
        }
    }

    for (size_t c = 0; c < 64; ++c) {
        if (c < num_channels) {
            if (has_custom_panning) {
                stream.write_u8(song.channel_panning[c]);
            } else {
                stream.write_u8(32); // Default center pan for active channel
            }
        } else {
            stream.write_u8(128); // Muted / inactive channel
        }
    }

    // Channel volume (64 bytes)
    bool has_custom_volume = false;
    for (size_t c = 0; c < num_channels; ++c) {
        if (song.channel_volume[c] != 0) {
            has_custom_volume = true;
            break;
        }
    }

    for (size_t c = 0; c < 64; ++c) {
        if (has_custom_volume && c < num_channels) {
            stream.write_u8(song.channel_volume[c]);
        } else {
            stream.write_u8(64);
        }
    }

    // 5. Write Orders
    for (uint16_t o = 0; o < ordnum; ++o) {
        stream.write_u8(song.order_table[o]);
    }

    // 6. Write Pointer Tables
    for (uint16_t i = 0; i < insnum; ++i) {
        stream.write_u32_le(inst_offsets[i]);
    }
    for (uint16_t s = 0; s < smpnum; ++s) {
        stream.write_u32_le(smp_offsets[s]);
    }
    for (uint16_t p = 0; p < patnum; ++p) {
        stream.write_u32_le(pat_offsets[p]);
    }

    // 7. Write Message Block
    if (has_msg && msg_length > 0) {
        stream.write(song.message.data(), song.message.size());
        stream.write_u8(0);
    }

    // 8. Write Instruments (554 bytes each)
    for (uint16_t i = 0; i < insnum; ++i) {
        const auto& inst = song.instruments[i];
        stream.write(IT_SIGNATURE_INST, IT_SIGNATURE_INST_LEN);
        stream.write_fixed_string(inst.filename, 12, '\0');
        stream.write_u8(0);
        stream.write_u8(static_cast<uint8_t>(inst.nna) & 3);
        stream.write_u8(static_cast<uint8_t>(inst.dct) & 3);
        stream.write_u8(static_cast<uint8_t>(inst.dca) & 3);
        stream.write_u16_le(inst.volume_fadeout);
        stream.write_u8(0);  // pitchpan_sep
        stream.write_u8(60); // pitchpan_center (C-5)
        stream.write_u8(inst.global_volume != 0 ? inst.global_volume : 128);

        if (inst.default_panning == 128) {
            stream.write_u8(128);
        } else {
            uint8_t p = static_cast<uint8_t>(inst.default_panning / 4);
            stream.write_u8(p > 63 ? 63 : p);
        }

        stream.write_u16_le(0); // random_var
        stream.write_u16_le(song.version != 0 ? song.version : 0x0214);
        stream.write_u8(static_cast<uint8_t>(inst.samples.size()));
        stream.write_u8(0); // reserved
        stream.write_fixed_string(inst.name, 26, '\0');
        stream.write_u16_le(0); // initial_filter
        stream.write_u32_le(0); // midi_settings

        // Keyboard Map (120 pairs)
        uint8_t sample_base = static_cast<uint8_t>(sample_offset_for_inst[i]);
        size_t num_inst_samples = inst.samples.size();
        bool has_explicit_keys = false;
        for (size_t k = 0; k < 120; ++k) {
            if (inst.keyboard_map[k].sample != 0) {
                has_explicit_keys = true;
                break;
            }
        }

        for (size_t k = 0; k < 120; ++k) {
            uint8_t note = (inst.keyboard_map[k].note != 0 || k == 0) ? inst.keyboard_map[k].note : static_cast<uint8_t>(k);
            uint8_t s_out = 0;
            if (has_explicit_keys) {
                uint8_t smp_idx = inst.keyboard_map[k].sample;
                if (smp_idx > 0 && smp_idx <= num_inst_samples) {
                    s_out = static_cast<uint8_t>(sample_base + smp_idx);
                } else if (smp_idx > 0 && smp_idx <= smpnum) {
                    s_out = smp_idx;
                } else if (num_inst_samples > 0) {
                    s_out = static_cast<uint8_t>(sample_base + 1);
                }
            } else {
                if (num_inst_samples > 0) {
                    if (k < 96 && inst.sample_map[k] < num_inst_samples) {
                        s_out = static_cast<uint8_t>(sample_base + inst.sample_map[k] + 1);
                    } else {
                        s_out = static_cast<uint8_t>(sample_base + 1);
                    }
                }
            }
            stream.write_u8(note);
            stream.write_u8(s_out);
        }

        // Envelope Helper (82 bytes)
        auto write_envelope = [&stream](const Envelope& env) {
            uint8_t env_flags = (env.enabled ? 1 : 0) |
                                (env.loop_enabled ? 2 : 0) |
                                (env.sustain_enabled ? 4 : 0);
            stream.write_u8(env_flags);
            uint8_t num_nodes = static_cast<uint8_t>(std::min<size_t>(env.points.size(), 25));
            stream.write_u8(num_nodes);
            stream.write_u8(env.loop_start_point);
            stream.write_u8(env.loop_end_point);
            stream.write_u8(env.sustain_point);
            stream.write_u8(env.sustain_point);
            for (size_t n = 0; n < 25; ++n) {
                if (n < num_nodes) {
                    stream.write_u8(static_cast<uint8_t>(env.points[n].value));
                    stream.write_u16_le(env.points[n].tick);
                } else {
                    stream.write_u8(0);
                    stream.write_u16_le(0);
                }
            }
            stream.write_u8(0); // padding
        };

        write_envelope(inst.volume_envelope);
        write_envelope(inst.panning_envelope);
        write_envelope(inst.pitch_envelope);

        stream.write_u32_le(0); // 4 reserved bytes
    }

    // 9. Write Sample Headers (80 bytes each)
    for (uint16_t s = 0; s < smpnum; ++s) {
        const auto& smp = *global_samples[s];
        stream.write(IT_SIGNATURE_SAMPLE, IT_SIGNATURE_SAMPLE_LEN);
        stream.write_fixed_string("", 12, '\0');
        stream.write_u8(0);
        stream.write_u8(smp.global_volume != 0 ? smp.global_volume : 64);

        uint32_t len = 0;
        if (smp.is_16bit) {
            len = smp.data16.empty() ? smp.length : static_cast<uint32_t>(smp.data16.size());
        } else {
            len = smp.data8.empty() ? smp.length : static_cast<uint32_t>(smp.data8.size());
        }

        uint8_t smp_flags = 0;
        if (len > 0 && (!smp.data8.empty() || !smp.data16.empty())) {
            smp_flags |= IT_SAMPLE_EXISTS;
        }
        if (smp.is_16bit) {
            smp_flags |= IT_SAMPLE_16BIT;
        }
        if (smp.loop_type == LoopType::Forward) {
            smp_flags |= IT_SAMPLE_LOOP;
        } else if (smp.loop_type == LoopType::PingPong) {
            smp_flags |= IT_SAMPLE_LOOP | IT_SAMPLE_PINGPONG_LOOP;
        }
        if (smp.sustain_loop_type == LoopType::Forward) {
            smp_flags |= IT_SAMPLE_SUSTAIN_LOOP;
        } else if (smp.sustain_loop_type == LoopType::PingPong) {
            smp_flags |= IT_SAMPLE_SUSTAIN_LOOP | IT_SAMPLE_PINGPONG_SUSTAIN;
        }
        stream.write_u8(smp_flags);
        stream.write_u8(smp.volume);
        stream.write_fixed_string(smp.name, 26, '\0');
        stream.write_u8(IT_CONVERT_SIGNED);

        uint8_t default_pan = 0;
        if (smp.panning != 128) {
            uint8_t p = static_cast<uint8_t>(smp.panning / 4);
            default_pan = 0x80 | (p > 64 ? 64 : p);
        }
        stream.write_u8(default_pan);

        stream.write_u32_le(len);
        stream.write_u32_le(smp.loop_start);
        stream.write_u32_le(smp.loop_start + smp.loop_length);
        stream.write_u32_le(smp.c5_speed != 0 ? smp.c5_speed : 8363);
        stream.write_u32_le(smp.sustain_loop_start);
        stream.write_u32_le(smp.sustain_loop_start + smp.sustain_loop_length);
        stream.write_u32_le(smp_pcm_offsets[s]);
        stream.write_u8(smp.vibrato_sweep);
        stream.write_u8(smp.vibrato_depth);
        stream.write_u8(smp.vibrato_rate);
        stream.write_u8(smp.vibrato_type);
    }

    // 10. Write Patterns
    for (uint16_t p = 0; p < patnum; ++p) {
        const auto& pat = song.patterns[p];
        const auto& packed = packed_patterns[p];
        uint16_t pat_len = static_cast<uint16_t>(packed.size());
        uint16_t num_rows = pat.num_rows != 0 ? pat.num_rows : 64;

        stream.write_u16_le(pat_len);
        stream.write_u16_le(num_rows);
        stream.write_u32_le(0); // 4 reserved bytes
        if (!packed.empty()) {
            stream.write(packed.data(), packed.size());
        }
    }

    // 11. Write Sample PCM Audio Data
    for (uint16_t s = 0; s < smpnum; ++s) {
        const auto& smp = *global_samples[s];
        if (smp.is_16bit) {
            for (int16_t val : smp.data16) {
                stream.write_i16_le(val);
            }
        } else {
            for (int8_t val : smp.data8) {
                stream.write_i8(val);
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::it
