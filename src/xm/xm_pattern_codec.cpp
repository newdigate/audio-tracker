#include <tracker/xm/xm_pattern_codec.hpp>
#include <tracker/xm/xm_types.hpp>

namespace tracker::xm {

std::vector<uint8_t> pack_pattern(const Pattern& pattern) {
    if (pattern.is_all_empty()) {
        return {};
    }

    std::vector<uint8_t> buffer;
    buffer.reserve(static_cast<size_t>(pattern.num_rows) * pattern.num_channels * 2);

    for (uint16_t row = 0; row < pattern.num_rows; ++row) {
        for (uint16_t ch = 0; ch < pattern.num_channels; ++ch) {
            const Cell& cell = pattern.get_cell(row, ch);
            if (cell.is_empty()) {
                buffer.push_back(XM_PACK_FLAG);
                continue;
            }

            uint8_t mask = XM_PACK_FLAG;
            if (cell.note != 0) mask |= XM_PACK_NOTE;
            if (cell.instrument != 0) mask |= XM_PACK_INSTRUMENT;
            if (cell.volume != 0) mask |= XM_PACK_VOLUME;
            if (cell.effect_type != 0) mask |= XM_PACK_EFFECT_TYPE;
            if (cell.effect_param != 0) mask |= XM_PACK_EFFECT_PARAM;

            buffer.push_back(mask);
            if (cell.note != 0) buffer.push_back(cell.note);
            if (cell.instrument != 0) buffer.push_back(cell.instrument);
            if (cell.volume != 0) buffer.push_back(cell.volume);
            if (cell.effect_type != 0) buffer.push_back(cell.effect_type);
            if (cell.effect_param != 0) buffer.push_back(cell.effect_param);
        }
    }
    return buffer;
}

Status unpack_pattern(const uint8_t* packed_data, size_t packed_size, Pattern& out_pattern) {
    size_t offset = 0;

    for (uint16_t row = 0; row < out_pattern.num_rows; ++row) {
        for (uint16_t ch = 0; ch < out_pattern.num_channels; ++ch) {
            Cell& cell = out_pattern.get_cell(row, ch);
            cell = Cell{};
            if (offset >= packed_size) {
                continue;
            }

            uint8_t b = packed_data[offset++];
            if (b & XM_PACK_FLAG) {
                if (b & XM_PACK_NOTE) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated note byte");
                    cell.note = packed_data[offset++];
                }
                if (b & XM_PACK_INSTRUMENT) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated instrument byte");
                    cell.instrument = packed_data[offset++];
                }
                if (b & XM_PACK_VOLUME) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated volume byte");
                    cell.volume = packed_data[offset++];
                }
                if (b & XM_PACK_EFFECT_TYPE) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated effect type byte");
                    cell.effect_type = packed_data[offset++];
                }
                if (b & XM_PACK_EFFECT_PARAM) {
                    if (offset >= packed_size) return Status::error(ErrorCode::CorruptPatternData, "Truncated effect param byte");
                    cell.effect_param = packed_data[offset++];
                }
            } else {
                cell.note = b;
                if (offset + 4 > packed_size) {
                    return Status::error(ErrorCode::CorruptPatternData, "Truncated uncompressed cell");
                }
                cell.instrument = packed_data[offset++];
                cell.volume = packed_data[offset++];
                cell.effect_type = packed_data[offset++];
                cell.effect_param = packed_data[offset++];
            }
        }
    }
    return Status::ok();
}

} // namespace tracker::xm
