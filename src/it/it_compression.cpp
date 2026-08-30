#include <tracker/it/it_compression.hpp>
#include <tracker/it/it_types.hpp>
#include <algorithm>

namespace tracker::it {

namespace {

class LsbBitReader {
public:
    LsbBitReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    uint32_t read_bits(uint32_t count) {
        if (count == 0) return 0;
        while (m_bit_count < count) {
            if (m_pos < m_size) {
                m_bit_buffer |= (static_cast<uint64_t>(m_data[m_pos++]) << m_bit_count);
            }
            m_bit_count += 8;
        }
        uint64_t mask = (count == 64) ? ~0ULL : ((1ULL << count) - 1ULL);
        uint32_t result = static_cast<uint32_t>(m_bit_buffer & mask);
        m_bit_buffer >>= count;
        m_bit_count -= count;
        return result;
    }

private:
    const uint8_t* m_data;
    size_t m_size{0};
    size_t m_pos{0};
    uint64_t m_bit_buffer{0};
    uint32_t m_bit_count{0};
};

} // namespace

Status decompress_it_sample_8(io::InputStream& stream,
                              std::vector<int8_t>& out_data,
                              uint32_t length_frames,
                              bool is_it215,
                              bool is_delta) {
    if (length_frames == 0) {
        out_data.clear();
        return Status::ok();
    }

    out_data.resize(length_frames, 0);
    uint32_t samples_written = 0;

    while (samples_written < length_frames) {
        uint32_t block_samples = std::min<uint32_t>(IT_COMPRESSION_BLOCK_SIZE_8, length_frames - samples_written);

        if (stream.eof()) {
            return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF before IT compressed block header");
        }
        uint16_t compressed_bytes = stream.read_u16_le();

        std::vector<uint8_t> block_buf(compressed_bytes);
        size_t bytes_read = stream.read(block_buf.data(), compressed_bytes);
        if (bytes_read < compressed_bytes) {
            return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading IT compressed block data");
        }

        LsbBitReader reader(block_buf.data(), block_buf.size());
        uint32_t width = 9;
        int8_t s = 0;
        int8_t d = 0;
        uint32_t block_out_idx = 0;

        while (block_out_idx < block_samples) {
            if (width > 9) {
                return Status::error(ErrorCode::CorruptSampleData, "Invalid bit width in 8-bit IT sample decompression");
            }
            uint32_t value = reader.read_bits(width);

            if (width <= 6) {
                // Type A
                uint32_t special_val = 1U << (width - 1);
                if (value == special_val) {
                    uint32_t extra = reader.read_bits(3);
                    uint32_t new_width = extra + 1;
                    if (new_width >= width) new_width++;
                    width = new_width;
                    continue;
                }
                int32_t delta;
                if (value > special_val) {
                    delta = static_cast<int32_t>(value) - (1 << width);
                } else {
                    delta = static_cast<int32_t>(value);
                }

                if (is_it215) {
                    d += static_cast<int8_t>(delta);
                    s += d;
                } else if (is_delta) {
                    s += static_cast<int8_t>(delta);
                } else {
                    s = static_cast<int8_t>(delta);
                }
                out_data[samples_written + block_out_idx++] = s;
            } else if (width <= 8) {
                // Type B
                uint32_t all_bits = 0xFF;
                uint32_t border = ((all_bits >> (8 + 1 - width)) - 4) & all_bits;
                if (value > border && value <= border + 8) {
                    value -= border;
                    uint32_t new_width = value;
                    if (new_width >= width) new_width++;
                    width = new_width;
                    continue;
                }
                if (value > border + 8) {
                    value -= 8;
                }
                int32_t delta;
                if (value & (1U << (width - 1))) {
                    delta = static_cast<int32_t>(value) - (1 << width);
                } else {
                    delta = static_cast<int32_t>(value);
                }

                if (is_it215) {
                    d += static_cast<int8_t>(delta);
                    s += d;
                } else if (is_delta) {
                    s += static_cast<int8_t>(delta);
                } else {
                    s = static_cast<int8_t>(delta);
                }
                out_data[samples_written + block_out_idx++] = s;
            } else {
                // Type C (width == 9)
                if (value & 0x100) {
                    width = (value & 0xFF) + 1;
                    continue;
                }
                int8_t delta = static_cast<int8_t>(value & 0xFF);
                if (is_it215) {
                    d += delta;
                    s += d;
                } else if (is_delta) {
                    s += delta;
                } else {
                    s = delta;
                }
                out_data[samples_written + block_out_idx++] = s;
            }
        }
        samples_written += block_samples;
    }

    return Status::ok();
}

Status decompress_it_sample_16(io::InputStream& stream,
                               std::vector<int16_t>& out_data,
                               uint32_t length_frames,
                               bool is_it215,
                               bool is_delta) {
    if (length_frames == 0) {
        out_data.clear();
        return Status::ok();
    }

    out_data.resize(length_frames, 0);
    uint32_t samples_written = 0;

    while (samples_written < length_frames) {
        uint32_t block_samples = std::min<uint32_t>(IT_COMPRESSION_BLOCK_SIZE_16, length_frames - samples_written);

        if (stream.eof()) {
            return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF before IT compressed block header");
        }
        uint16_t compressed_bytes = stream.read_u16_le();

        std::vector<uint8_t> block_buf(compressed_bytes);
        size_t bytes_read = stream.read(block_buf.data(), compressed_bytes);
        if (bytes_read < compressed_bytes) {
            return Status::error(ErrorCode::UnexpectedEof, "Unexpected EOF reading IT compressed block data");
        }

        LsbBitReader reader(block_buf.data(), block_buf.size());
        uint32_t width = 17;
        int16_t s = 0;
        int16_t d = 0;
        uint32_t block_out_idx = 0;

        while (block_out_idx < block_samples) {
            if (width > 17) {
                return Status::error(ErrorCode::CorruptSampleData, "Invalid bit width in 16-bit IT sample decompression");
            }
            uint32_t value = reader.read_bits(width);

            if (width <= 6) {
                // Type A
                uint32_t special_val = 1U << (width - 1);
                if (value == special_val) {
                    uint32_t extra = reader.read_bits(4);
                    uint32_t new_width = extra + 1;
                    if (new_width >= width) new_width++;
                    width = new_width;
                    continue;
                }
                int32_t delta;
                if (value > special_val) {
                    delta = static_cast<int32_t>(value) - (1 << width);
                } else {
                    delta = static_cast<int32_t>(value);
                }

                if (is_it215) {
                    d += static_cast<int16_t>(delta);
                    s += d;
                } else if (is_delta) {
                    s += static_cast<int16_t>(delta);
                } else {
                    s = static_cast<int16_t>(delta);
                }
                out_data[samples_written + block_out_idx++] = s;
            } else if (width <= 16) {
                // Type B
                uint32_t all_bits = 0xFFFF;
                uint32_t border = ((all_bits >> (16 + 1 - width)) - 8) & all_bits;
                if (value > border && value <= border + 16) {
                    value -= border;
                    uint32_t new_width = value;
                    if (new_width >= width) new_width++;
                    width = new_width;
                    continue;
                }
                if (value > border + 16) {
                    value -= 16;
                }
                int32_t delta;
                if (value & (1U << (width - 1))) {
                    delta = static_cast<int32_t>(value) - (1 << width);
                } else {
                    delta = static_cast<int32_t>(value);
                }

                if (is_it215) {
                    d += static_cast<int16_t>(delta);
                    s += d;
                } else if (is_delta) {
                    s += static_cast<int16_t>(delta);
                } else {
                    s = static_cast<int16_t>(delta);
                }
                out_data[samples_written + block_out_idx++] = s;
            } else {
                // Type C (width == 17)
                if (value & 0x10000) {
                    width = (value & 0xFFFF) + 1;
                    continue;
                }
                int16_t delta = static_cast<int16_t>(value & 0xFFFF);
                if (is_it215) {
                    d += delta;
                    s += d;
                } else if (is_delta) {
                    s += delta;
                } else {
                    s = delta;
                }
                out_data[samples_written + block_out_idx++] = s;
            }
        }
        samples_written += block_samples;
    }

    return Status::ok();
}

} // namespace tracker::it
