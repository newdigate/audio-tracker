#include <tracker/s3m/s3m_pattern_codec.hpp>
#include <tracker/s3m/s3m_types.hpp>
#include <algorithm>

namespace tracker::s3m {

Status unpack_pattern(io::InputStream& stream, Pattern& out_pat, uint16_t num_channels) {
    if (num_channels == 0) {
        return Status::error(ErrorCode::InvalidChannelCount, "Channel count cannot be 0");
    }

    out_pat = Pattern(static_cast<uint16_t>(S3M_ROWS_PER_PATTERN), num_channels);

    for (uint16_t row = 0; row < S3M_ROWS_PER_PATTERN; ++row) {
        while (true) {
            uint8_t channel_control = 0;
            if (stream.read(&channel_control, 1) != 1) {
                return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading channel control in S3M pattern");
            }

            if (channel_control == S3M_ROW_END) {
                break;
            }

            uint8_t channel = channel_control & S3M_PACK_CHANNEL_MASK;

            uint8_t note = 0;
            uint8_t instrument = 0;
            uint8_t volume = 0;
            uint8_t effect_type = 0;
            uint8_t effect_param = 0;

            if (channel_control & S3M_PACK_NOTE_INST) {
                uint8_t note_inst_bytes[2] = {0, 0};
                if (stream.read(note_inst_bytes, 2) != 2) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading note and instrument in S3M pattern");
                }
                note = s3m_byte_to_note(note_inst_bytes[0]);
                instrument = note_inst_bytes[1];
            }

            if (channel_control & S3M_PACK_VOLUME) {
                if (stream.read(&volume, 1) != 1) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading volume in S3M pattern");
                }
            }

            if (channel_control & S3M_PACK_EFFECT) {
                uint8_t eff_bytes[2] = {0, 0};
                if (stream.read(eff_bytes, 2) != 2) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading effect in S3M pattern");
                }
                effect_type = eff_bytes[0];
                effect_param = eff_bytes[1];
            }

            if (channel < num_channels) {
                Cell& cell = out_pat.get_cell(row, channel);
                if (channel_control & S3M_PACK_NOTE_INST) {
                    cell.note = note;
                    cell.instrument = instrument;
                }
                if (channel_control & S3M_PACK_VOLUME) {
                    cell.volume = volume;
                }
                if (channel_control & S3M_PACK_EFFECT) {
                    cell.effect_type = effect_type;
                    cell.effect_param = effect_param;
                }
            }
        }
    }

    return Status::ok();
}

Status pack_pattern(const Pattern& pat, io::OutputStream& stream) {
    uint16_t channels_to_pack = std::min<uint16_t>(pat.num_channels, static_cast<uint16_t>(S3M_MAX_CHANNELS));

    for (uint16_t row = 0; row < S3M_ROWS_PER_PATTERN; ++row) {
        if (row < pat.num_rows) {
            for (uint16_t ch = 0; ch < channels_to_pack; ++ch) {
                const Cell& cell = pat.get_cell(row, ch);
                if (cell.is_empty()) {
                    continue;
                }

                uint8_t ctrl = static_cast<uint8_t>(ch & S3M_PACK_CHANNEL_MASK);
                bool has_note_inst = (cell.note != 0 || cell.instrument != 0);
                bool has_volume = (cell.volume != 0);
                bool has_effect = (cell.effect_type != 0 || cell.effect_param != 0);

                if (!has_note_inst && !has_volume && !has_effect) {
                    continue;
                }

                if (has_note_inst) {
                    ctrl |= S3M_PACK_NOTE_INST;
                }
                if (has_volume) {
                    ctrl |= S3M_PACK_VOLUME;
                }
                if (has_effect) {
                    ctrl |= S3M_PACK_EFFECT;
                }

                stream.write_u8(ctrl);

                if (has_note_inst) {
                    stream.write_u8(note_to_s3m_byte(cell.note));
                    stream.write_u8(cell.instrument);
                }

                if (has_volume) {
                    stream.write_u8(cell.volume);
                }

                if (has_effect) {
                    stream.write_u8(cell.effect_type);
                    stream.write_u8(cell.effect_param);
                }
            }
        }

        stream.write_u8(S3M_ROW_END);
    }

    return Status::ok();
}

} // namespace tracker::s3m
