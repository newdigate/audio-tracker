#include <tracker/s3m/s3m_writer.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <algorithm>
#include <vector>
#include <array>

namespace tracker::s3m {

Result<std::vector<uint8_t>> S3mWriter::save_to_memory(const Song& song) {
    io::MemoryOutputStream stream(16384);
    Status s = save(song, stream);
    if (!s.is_ok()) return Result<std::vector<uint8_t>>(s);
    return Result<std::vector<uint8_t>>(stream.take_data());
}

Status S3mWriter::save_to_file(const Song& song, const std::string& path) {
    auto out_res = io::FileOutputStream::open(path);
    if (!out_res.is_ok()) return out_res.status();
    return save(song, out_res.value());
}

Status S3mWriter::save(const Song& song, io::OutputStream& stream) {
    uint16_t num_channels = std::min<uint16_t>(std::max<uint16_t>(song.num_channels, 1), static_cast<uint16_t>(S3M_MAX_CHANNELS));
    uint16_t ordnum = static_cast<uint16_t>(std::min<size_t>(song.order_table.size(), S3M_MAX_ORDERS));
    uint16_t insnum = static_cast<uint16_t>(std::min<size_t>(song.instruments.size(), S3M_MAX_INSTRUMENTS));
    uint16_t patnum = static_cast<uint16_t>(std::min<size_t>(song.patterns.size(), S3M_MAX_PATTERNS));

    // 1. Identify valid instruments
    std::vector<bool> has_sample(insnum, false);
    for (uint16_t i = 0; i < insnum; ++i) {
        if (!song.instruments[i].samples.empty()) {
            has_sample[i] = true;
        }
    }

    // 2. Pre-pack patterns
    std::vector<std::vector<uint8_t>> packed_patterns(patnum);
    for (uint16_t p = 0; p < patnum; ++p) {
        io::MemoryOutputStream pat_stream;
        auto pack_res = pack_pattern(song.patterns[p], pat_stream);
        if (!pack_res.is_ok()) return pack_res;
        packed_patterns[p] = pat_stream.take_data();
    }

    // 3. Compute file offsets and parapointers
    uint32_t current_offset = S3M_HEADER_SIZE + ordnum + static_cast<uint32_t>((insnum + patnum) * 2) + 32;

    std::vector<uint32_t> ins_offsets(insnum, 0);
    std::vector<uint16_t> ins_parapointers(insnum, 0);
    for (uint16_t i = 0; i < insnum; ++i) {
        if (has_sample[i]) {
            current_offset = align_paragraph_offset(current_offset);
            ins_offsets[i] = current_offset;
            ins_parapointers[i] = offset_to_parapointer(current_offset);
            current_offset += S3M_SAMPLE_HEADER_SIZE;
        }
    }

    std::vector<uint32_t> pat_offsets(patnum, 0);
    std::vector<uint16_t> pat_parapointers(patnum, 0);
    for (uint16_t p = 0; p < patnum; ++p) {
        current_offset = align_paragraph_offset(current_offset);
        pat_offsets[p] = current_offset;
        pat_parapointers[p] = offset_to_parapointer(current_offset);
        current_offset += 2 + static_cast<uint32_t>(packed_patterns[p].size());
    }

    std::vector<uint32_t> smp_pcm_offsets(insnum, 0);
    for (uint16_t i = 0; i < insnum; ++i) {
        if (has_sample[i]) {
            const auto& smp = song.instruments[i].samples[0];
            uint32_t pcm_bytes = 0;
            if (smp.is_16bit) {
                pcm_bytes = static_cast<uint32_t>(smp.data16.size() * 2);
            } else {
                pcm_bytes = static_cast<uint32_t>(smp.data8.size());
            }
            if (pcm_bytes > 0) {
                current_offset = align_paragraph_offset(current_offset);
                smp_pcm_offsets[i] = current_offset;
                current_offset += pcm_bytes;
            }
        }
    }

    // 4. Configure Channel Settings and Custom Panning Table
    std::array<uint8_t, 32> channel_settings{};
    for (size_t c = 0; c < 32; ++c) {
        if (c < num_channels) {
            channel_settings[c] = static_cast<uint8_t>(c);
        } else {
            channel_settings[c] = S3M_CHANNEL_DISABLED;
        }
    }

    bool has_custom_panning = false;
    for (size_t c = 0; c < num_channels; ++c) {
        if (song.channel_panning[c] != 0) {
            has_custom_panning = true;
            break;
        }
    }

    std::array<uint8_t, 32> custom_pan_table{};
    for (size_t c = 0; c < 32; ++c) {
        if (c < num_channels) {
            if (has_custom_panning) {
                uint8_t pan_4bit = std::min<uint8_t>(song.channel_panning[c] / 4, 15);
                custom_pan_table[c] = 0x20 | pan_4bit;
            } else {
                custom_pan_table[c] = 0x00;
            }
        } else {
            custom_pan_table[c] = 0x00;
        }
    }

    // 5. Write S3M Header (96 bytes)
    stream.write_fixed_string(song.name, 28, '\0');
    stream.write_u8(0x1A);   // dos_eof
    stream.write_u8(0x10);   // file_type = ST3 module
    stream.write_u16_le(0);  // reserved1
    stream.write_u16_le(ordnum);
    stream.write_u16_le(insnum);
    stream.write_u16_le(patnum);
    stream.write_u16_le(0);  // flags
    stream.write_u16_le(song.version != 0 ? song.version : 0x1320); // cwt_vwt
    stream.write_u16_le(S3M_FFI_UNSIGNED); // ffi = 2
    stream.write(S3M_SIGNATURE_SONG, S3M_SIGNATURE_SONG_LEN); // "SCRM"
    stream.write_u8(song.global_volume != 0 ? (song.global_volume > 64 ? 64 : song.global_volume) : 64);
    stream.write_u8(song.default_speed != 0 ? static_cast<uint8_t>(song.default_speed) : 6);
    stream.write_u8(song.default_bpm != 0 ? static_cast<uint8_t>(song.default_bpm) : 125);
    stream.write_u8((song.mix_volume != 0 ? (song.mix_volume & 0x7F) : 48) | 0x80); // Stereo
    stream.write_u8(0);      // ultraclick
    stream.write_u8(S3M_DEFAULT_PANNING_TAG); // default_pan_tag = 0xFC
    stream.write_zeros(8);   // reserved2
    stream.write_u16_le(0);  // special_ptr
    stream.write(channel_settings.data(), 32);

    size_t written_bytes = S3M_HEADER_SIZE;

    // 6. Write Orders
    for (uint16_t o = 0; o < ordnum; ++o) {
        stream.write_u8(song.order_table[o]);
    }
    written_bytes += ordnum;

    // 7. Write Parapointers
    for (uint16_t i = 0; i < insnum; ++i) {
        stream.write_u16_le(ins_parapointers[i]);
    }
    written_bytes += insnum * 2;

    for (uint16_t p = 0; p < patnum; ++p) {
        stream.write_u16_le(pat_parapointers[p]);
    }
    written_bytes += patnum * 2;

    // 8. Write Custom Panning Table
    stream.write(custom_pan_table.data(), 32);
    written_bytes += 32;

    // 9. Write Sample Headers (80 bytes each)
    for (uint16_t i = 0; i < insnum; ++i) {
        if (ins_parapointers[i] != 0) {
            if (ins_offsets[i] > written_bytes) {
                size_t pad = ins_offsets[i] - written_bytes;
                stream.write_zeros(pad);
                written_bytes += pad;
            }

            const auto& inst = song.instruments[i];
            const auto& smp = inst.samples[0];

            stream.write_u8(S3M_SAMPLE_TYPE_PCM);
            stream.write_fixed_string(inst.filename, 12, '\0');

            uint32_t pcm_pp = offset_to_parapointer(smp_pcm_offsets[i]);
            uint8_t memseg_hi = static_cast<uint8_t>((pcm_pp >> 16) & 0xFF);
            uint16_t memseg_lo = static_cast<uint16_t>(pcm_pp & 0xFFFF);
            stream.write_u8(memseg_hi);
            stream.write_u16_le(memseg_lo);

            uint32_t len = 0;
            if (smp.is_16bit) {
                len = smp.data16.empty() ? smp.length : static_cast<uint32_t>(smp.data16.size());
            } else {
                len = smp.data8.empty() ? smp.length : static_cast<uint32_t>(smp.data8.size());
            }
            stream.write_u32_le(len);

            uint32_t loop_start = (smp.loop_type != LoopType::None) ? smp.loop_start : 0;
            uint32_t loop_end = (smp.loop_type != LoopType::None) ? (smp.loop_start + smp.loop_length) : 0;
            stream.write_u32_le(loop_start);
            stream.write_u32_le(loop_end);

            stream.write_u8(smp.volume > 64 ? 64 : smp.volume);
            stream.write_u8(0); // dsk
            stream.write_u8(0); // pack

            uint8_t flags_smp = 0;
            if (smp.loop_type != LoopType::None) flags_smp |= S3M_SAMPLE_LOOP;
            if (smp.is_16bit) flags_smp |= S3M_SAMPLE_16BIT;
            stream.write_u8(flags_smp);

            stream.write_u32_le(smp.c5_speed != 0 ? smp.c5_speed : 8363);
            stream.write_zeros(12); // reserved

            std::string sample_name = smp.name.empty() ? inst.name : smp.name;
            stream.write_fixed_string(sample_name, 28, '\0');
            stream.write(S3M_SIGNATURE_SAMPLE, S3M_SIGNATURE_SAMPLE_LEN); // "SCRS"

            written_bytes += S3M_SAMPLE_HEADER_SIZE;
        }
    }

    // 10. Write Patterns
    for (uint16_t p = 0; p < patnum; ++p) {
        if (pat_offsets[p] > written_bytes) {
            size_t pad = pat_offsets[p] - written_bytes;
            stream.write_zeros(pad);
            written_bytes += pad;
        }

        uint16_t packed_len = static_cast<uint16_t>(packed_patterns[p].size());
        stream.write_u16_le(packed_len);
        if (!packed_patterns[p].empty()) {
            stream.write(packed_patterns[p].data(), packed_patterns[p].size());
        }

        written_bytes += 2 + packed_patterns[p].size();
    }

    // 11. Write Sample PCM Audio Data
    for (uint16_t i = 0; i < insnum; ++i) {
        if (smp_pcm_offsets[i] != 0) {
            if (smp_pcm_offsets[i] > written_bytes) {
                size_t pad = smp_pcm_offsets[i] - written_bytes;
                stream.write_zeros(pad);
                written_bytes += pad;
            }

            const auto& smp = song.instruments[i].samples[0];
            if (smp.is_16bit) {
                for (int16_t val : smp.data16) {
                    uint16_t u16 = static_cast<uint16_t>(val ^ 0x8000);
                    stream.write_u16_le(u16);
                }
                written_bytes += smp.data16.size() * 2;
            } else {
                for (int8_t val : smp.data8) {
                    uint8_t u8 = static_cast<uint8_t>(val ^ 0x80);
                    stream.write_u8(u8);
                }
                written_bytes += smp.data8.size();
            }
        }
    }

    return Status::ok();
}

} // namespace tracker::s3m
