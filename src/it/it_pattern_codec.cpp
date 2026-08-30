#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/it/it_types.hpp>
#include <array>
#include <algorithm>

namespace tracker::it {

Status unpack_pattern(io::InputStream& stream, Pattern& out_pat, uint16_t num_rows, uint16_t num_channels) {
    if (num_channels == 0) {
        return Status::error(ErrorCode::InvalidChannelCount, "Channel count cannot be 0");
    }

    out_pat = Pattern(num_rows, num_channels);

    std::array<uint8_t, IT_MAX_CHANNELS> last_mask{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_note{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_instrument{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_volume{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_effect_type{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_effect_param{};

    for (uint16_t row = 0; row < num_rows; ++row) {
        while (true) {
            uint8_t channel_var = 0;
            if (stream.read(&channel_var, 1) != 1) {
                return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading channel variable in IT pattern");
            }

            if (channel_var == IT_ROW_END) {
                break;
            }

            uint8_t ch_num = channel_var & IT_CHANNEL_NUM_MASK;
            if (ch_num == 0 || ch_num > IT_MAX_CHANNELS) {
                return Status::error(ErrorCode::CorruptPatternData, "Invalid channel index in IT pattern data");
            }
            uint8_t ch = static_cast<uint8_t>(ch_num - 1);

            uint8_t mask = last_mask[ch];
            if (channel_var & IT_CHANNEL_HAS_MASK) {
                if (stream.read(&mask, 1) != 1) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading mask variable in IT pattern");
                }
                last_mask[ch] = mask;
            }

            uint8_t note = 0;
            uint8_t instrument = 0;
            uint8_t volume = 0;
            uint8_t effect_type = 0;
            uint8_t effect_param = 0;

            // Note (bit 0 = read, bit 4 = reuse)
            if (mask & IT_MASK_NOTE) {
                if (stream.read(&note, 1) != 1) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading note in IT pattern");
                }
                last_note[ch] = note;
            } else if (mask & IT_MASK_SAME_NOTE) {
                note = last_note[ch];
            }

            // Instrument (bit 1 = read, bit 5 = reuse)
            if (mask & IT_MASK_INSTRUMENT) {
                if (stream.read(&instrument, 1) != 1) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading instrument in IT pattern");
                }
                last_instrument[ch] = instrument;
            } else if (mask & IT_MASK_SAME_INSTRUMENT) {
                instrument = last_instrument[ch];
            }

            // Volume (bit 2 = read, bit 6 = reuse)
            if (mask & IT_MASK_VOLUME) {
                if (stream.read(&volume, 1) != 1) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading volume in IT pattern");
                }
                last_volume[ch] = volume;
            } else if (mask & IT_MASK_SAME_VOLUME) {
                volume = last_volume[ch];
            }

            // Effect (bit 3 = read, bit 7 = reuse)
            if (mask & IT_MASK_EFFECT) {
                uint8_t eff_bytes[2] = {0, 0};
                if (stream.read(eff_bytes, 2) != 2) {
                    return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading effect in IT pattern");
                }
                effect_type = eff_bytes[0];
                effect_param = eff_bytes[1];
                last_effect_type[ch] = effect_type;
                last_effect_param[ch] = effect_param;
            } else if (mask & IT_MASK_SAME_EFFECT) {
                effect_type = last_effect_type[ch];
                effect_param = last_effect_param[ch];
            }

            if (ch < num_channels) {
                Cell& cell = out_pat.get_cell(row, ch);
                cell.note = note;
                cell.instrument = instrument;
                cell.volume = volume;
                cell.effect_type = effect_type;
                cell.effect_param = effect_param;
            }
        }
    }

    return Status::ok();
}

Status pack_pattern(const Pattern& pat, io::OutputStream& stream) {
    std::array<uint8_t, IT_MAX_CHANNELS> last_mask{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_note{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_instrument{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_volume{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_effect_type{};
    std::array<uint8_t, IT_MAX_CHANNELS> last_effect_param{};

    uint16_t channels_to_pack = std::min<uint16_t>(pat.num_channels, static_cast<uint16_t>(IT_MAX_CHANNELS));

    for (uint16_t row = 0; row < pat.num_rows; ++row) {
        for (uint16_t ch = 0; ch < channels_to_pack; ++ch) {
            const Cell& cell = pat.get_cell(row, ch);
            if (cell.is_empty()) {
                continue;
            }

            uint8_t mask = 0;

            // Note
            if (cell.note != 0) {
                if (cell.note == last_note[ch] && last_note[ch] != 0) {
                    mask |= IT_MASK_SAME_NOTE;
                } else {
                    mask |= IT_MASK_NOTE;
                }
            }

            // Instrument
            if (cell.instrument != 0) {
                if (cell.instrument == last_instrument[ch] && last_instrument[ch] != 0) {
                    mask |= IT_MASK_SAME_INSTRUMENT;
                } else {
                    mask |= IT_MASK_INSTRUMENT;
                }
            }

            // Volume
            if (cell.volume != 0) {
                if (cell.volume == last_volume[ch] && last_volume[ch] != 0) {
                    mask |= IT_MASK_SAME_VOLUME;
                } else {
                    mask |= IT_MASK_VOLUME;
                }
            }

            // Effect
            if (cell.effect_type != 0 || cell.effect_param != 0) {
                if (cell.effect_type == last_effect_type[ch] &&
                    cell.effect_param == last_effect_param[ch] &&
                    (last_effect_type[ch] != 0 || last_effect_param[ch] != 0)) {
                    mask |= IT_MASK_SAME_EFFECT;
                } else {
                    mask |= IT_MASK_EFFECT;
                }
            }

            uint8_t channel_var = static_cast<uint8_t>(ch + 1);
            bool mask_changed = (mask != last_mask[ch]);
            if (mask_changed) {
                channel_var |= IT_CHANNEL_HAS_MASK;
            }

            stream.write_u8(channel_var);

            if (mask_changed) {
                stream.write_u8(mask);
                last_mask[ch] = mask;
            }

            if (mask & IT_MASK_NOTE) {
                stream.write_u8(cell.note);
                last_note[ch] = cell.note;
            }
            if (mask & IT_MASK_INSTRUMENT) {
                stream.write_u8(cell.instrument);
                last_instrument[ch] = cell.instrument;
            }
            if (mask & IT_MASK_VOLUME) {
                stream.write_u8(cell.volume);
                last_volume[ch] = cell.volume;
            }
            if (mask & IT_MASK_EFFECT) {
                stream.write_u8(cell.effect_type);
                stream.write_u8(cell.effect_param);
                last_effect_type[ch] = cell.effect_type;
                last_effect_param[ch] = cell.effect_param;
            }
        }

        stream.write_u8(IT_ROW_END);
    }

    return Status::ok();
}

} // namespace tracker::it
