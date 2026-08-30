#include "test_main.hpp"
#include <tracker/it/it_compression.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/io/memory_stream.hpp>
#include <vector>
#include <cmath>

namespace {

class LsbBitWriter {
public:
    void write_bits(uint32_t value, uint32_t count) {
        if (count == 0) return;
        m_buffer |= (static_cast<uint64_t>(value & ((1ULL << count) - 1ULL)) << m_bit_count);
        m_bit_count += count;
        while (m_bit_count >= 8) {
            m_data.push_back(static_cast<uint8_t>(m_buffer & 0xFF));
            m_buffer >>= 8;
            m_bit_count -= 8;
        }
    }

    void flush() {
        if (m_bit_count > 0) {
            m_data.push_back(static_cast<uint8_t>(m_buffer & 0xFF));
            m_buffer = 0;
            m_bit_count = 0;
        }
    }

    const std::vector<uint8_t>& data() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
    uint64_t m_buffer{0};
    uint32_t m_bit_count{0};
};

std::vector<uint8_t> compress_it_sample_8(const std::vector<int8_t>& pcm, bool is_it215, bool is_delta) {
    std::vector<uint8_t> result;
    size_t pos = 0;
    size_t total = pcm.size();

    while (pos < total) {
        size_t block_len = std::min<size_t>(tracker::it::IT_COMPRESSION_BLOCK_SIZE_8, total - pos);
        LsbBitWriter writer;
        uint32_t width = 9;
        int8_t s = 0;
        int8_t d = 0;

        for (size_t i = 0; i < block_len; ++i) {
            int8_t current = pcm[pos + i];
            int8_t delta;
            if (is_it215) {
                int8_t new_d = current - s;
                delta = new_d - d;
                d = new_d;
                s = current;
            } else if (is_delta) {
                delta = current - s;
                s = current;
            } else {
                delta = current;
            }

            if (width == 9) {
                if (delta >= -15 && delta <= 15 && i + 2 < block_len) {
                    // Switch to width 5: Type C escape
                    writer.write_bits(0x100 | (5 - 1), 9);
                    width = 5;
                } else {
                    writer.write_bits(static_cast<uint8_t>(delta), 9);
                    continue;
                }
            }

            if (width <= 6) {
                int32_t max_val = (1 << (width - 1)) - 1;
                int32_t min_val = -max_val;
                if (delta >= min_val && delta <= max_val) {
                    if (delta < 0) {
                        writer.write_bits(static_cast<uint32_t>(delta + (1 << width)), width);
                    } else {
                        writer.write_bits(static_cast<uint32_t>(delta), width);
                    }
                } else {
                    // Escape: special value = 1 << (width - 1)
                    writer.write_bits(1U << (width - 1), width);
                    uint32_t target_width = 9;
                    uint32_t extra = (target_width >= width) ? (target_width - 2) : (target_width - 1);
                    writer.write_bits(extra, 3);
                    width = target_width;
                    writer.write_bits(static_cast<uint8_t>(delta), 9);
                }
            } else if (width <= 8) {
                // Type B
                uint32_t all_bits = 0xFF;
                uint32_t border = ((all_bits >> (8 + 1 - width)) - 4) & all_bits;
                int32_t max_pos = static_cast<int32_t>(border);
                int32_t min_neg = -static_cast<int32_t>(1 << (width - 1));
                if (delta >= 0 && delta <= max_pos) {
                    writer.write_bits(static_cast<uint32_t>(delta), width);
                } else if (delta <= -9 && delta >= min_neg) {
                    int32_t val = delta + (1 << width) + 8;
                    writer.write_bits(static_cast<uint32_t>(val), width);
                } else {
                    // Escape back to 9
                    uint32_t target_width = 9;
                    uint32_t val = (target_width >= width) ? (target_width - 1) : target_width;
                    val += border;
                    writer.write_bits(val, width);
                    width = target_width;
                    writer.write_bits(static_cast<uint8_t>(delta), 9);
                }
            }
        }
        writer.flush();
        const auto& bytes = writer.data();
        uint16_t sz = static_cast<uint16_t>(bytes.size());
        result.push_back(static_cast<uint8_t>(sz & 0xFF));
        result.push_back(static_cast<uint8_t>((sz >> 8) & 0xFF));
        result.insert(result.end(), bytes.begin(), bytes.end());
        pos += block_len;
    }
    return result;
}

std::vector<uint8_t> compress_it_sample_16(const std::vector<int16_t>& pcm, bool is_it215, bool is_delta) {
    std::vector<uint8_t> result;
    size_t pos = 0;
    size_t total = pcm.size();

    while (pos < total) {
        size_t block_len = std::min<size_t>(tracker::it::IT_COMPRESSION_BLOCK_SIZE_16, total - pos);
        LsbBitWriter writer;
        uint32_t width = 17;
        int16_t s = 0;
        int16_t d = 0;

        for (size_t i = 0; i < block_len; ++i) {
            int16_t current = pcm[pos + i];
            int16_t delta;
            if (is_it215) {
                int16_t new_d = current - s;
                delta = new_d - d;
                d = new_d;
                s = current;
            } else if (is_delta) {
                delta = current - s;
                s = current;
            } else {
                delta = current;
            }

            if (width == 17) {
                if (delta >= -15 && delta <= 15 && i + 2 < block_len) {
                    // Switch to width 5
                    writer.write_bits(0x10000 | (5 - 1), 17);
                    width = 5;
                } else if (delta >= -200 && delta <= 200 && i + 2 < block_len) {
                    // Switch to width 10 (Type B)
                    writer.write_bits(0x10000 | (10 - 1), 17);
                    width = 10;
                } else {
                    writer.write_bits(static_cast<uint16_t>(delta), 17);
                    continue;
                }
            }

            if (width <= 6) {
                int32_t max_val = (1 << (width - 1)) - 1;
                int32_t min_val = -max_val;
                if (delta >= min_val && delta <= max_val) {
                    if (delta < 0) {
                        writer.write_bits(static_cast<uint32_t>(delta + (1 << width)), width);
                    } else {
                        writer.write_bits(static_cast<uint32_t>(delta), width);
                    }
                } else {
                    // Escape: special value = 1 << (width - 1)
                    writer.write_bits(1U << (width - 1), width);
                    uint32_t target_width = 17;
                    uint32_t extra = (target_width >= width) ? (target_width - 2) : (target_width - 1);
                    writer.write_bits(extra, 4);
                    width = target_width;
                    writer.write_bits(static_cast<uint16_t>(delta), 17);
                }
            } else if (width <= 16) {
                uint32_t all_bits = 0xFFFF;
                uint32_t border = ((all_bits >> (16 + 1 - width)) - 8) & all_bits;
                int32_t max_pos = static_cast<int32_t>(border);
                int32_t min_neg = -static_cast<int32_t>(1 << (width - 1));
                if (delta >= 0 && delta <= max_pos) {
                    writer.write_bits(static_cast<uint32_t>(delta), width);
                } else if (delta <= -17 && delta >= min_neg) {
                    int32_t val = delta + (1 << width) + 16;
                    writer.write_bits(static_cast<uint32_t>(val), width);
                } else {
                    // Escape back to 17
                    uint32_t target_width = 17;
                    uint32_t val = (target_width >= width) ? (target_width - 1) : target_width;
                    val += border;
                    writer.write_bits(val, width);
                    width = target_width;
                    writer.write_bits(static_cast<uint16_t>(delta), 17);
                }
            }
        }
        writer.flush();
        const auto& bytes = writer.data();
        uint16_t sz = static_cast<uint16_t>(bytes.size());
        result.push_back(static_cast<uint8_t>(sz & 0xFF));
        result.push_back(static_cast<uint8_t>((sz >> 8) & 0xFF));
        result.insert(result.end(), bytes.begin(), bytes.end());
        pos += block_len;
    }
    return result;
}

} // namespace

TEST_CASE(ItCompression_8BitEmptyAndSingle) {
    std::vector<int8_t> out;
    std::vector<uint8_t> empty_data;
    tracker::io::MemoryInputStream stream(empty_data.data(), empty_data.size());
    auto status = tracker::it::decompress_it_sample_8(stream, out, 0, false, false);
    REQUIRE(status.is_ok());
    REQUIRE(out.empty());
}

TEST_CASE(ItCompression_16BitEmpty) {
    std::vector<int16_t> out;
    std::vector<uint8_t> empty_data;
    tracker::io::MemoryInputStream stream(empty_data.data(), empty_data.size());
    auto status = tracker::it::decompress_it_sample_16(stream, out, 0, false, false);
    REQUIRE(status.is_ok());
    REQUIRE(out.empty());
}

TEST_CASE(ItCompression_8BitSingleDelta214) {
    std::vector<int8_t> original(500);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int8_t>(std::sin(i * 0.1) * 100.0);
    }

    auto compressed = compress_it_sample_8(original, false, true);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int8_t> decompressed;
    auto status = tracker::it::decompress_it_sample_8(stream, decompressed, static_cast<uint32_t>(original.size()), false, true);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_8BitDoubleDelta215) {
    std::vector<int8_t> original(1000);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int8_t>((i % 200) - 100);
    }

    auto compressed = compress_it_sample_8(original, true, false);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int8_t> decompressed;
    auto status = tracker::it::decompress_it_sample_8(stream, decompressed, static_cast<uint32_t>(original.size()), true, false);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_8BitMultiBlock) {
    // Multi-block test (> 32768 samples)
    std::vector<int8_t> original(40000);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int8_t>((i * 3) % 256 - 128);
    }

    auto compressed = compress_it_sample_8(original, false, true);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int8_t> decompressed;
    auto status = tracker::it::decompress_it_sample_8(stream, decompressed, static_cast<uint32_t>(original.size()), false, true);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_16BitSingleDelta214) {
    std::vector<int16_t> original(600);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int16_t>(std::sin(i * 0.05) * 25000.0);
    }

    auto compressed = compress_it_sample_16(original, false, true);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int16_t> decompressed;
    auto status = tracker::it::decompress_it_sample_16(stream, decompressed, static_cast<uint32_t>(original.size()), false, true);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_16BitDoubleDelta215) {
    std::vector<int16_t> original(1200);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int16_t>((i * 17) % 65536 - 32768);
    }

    auto compressed = compress_it_sample_16(original, true, false);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int16_t> decompressed;
    auto status = tracker::it::decompress_it_sample_16(stream, decompressed, static_cast<uint32_t>(original.size()), true, false);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_16BitMultiBlock) {
    // Multi-block test (> 16384 samples)
    std::vector<int16_t> original(20000);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int16_t>(i % 1000 - 500);
    }

    auto compressed = compress_it_sample_16(original, false, true);
    tracker::io::MemoryInputStream stream(compressed.data(), compressed.size());
    std::vector<int16_t> decompressed;
    auto status = tracker::it::decompress_it_sample_16(stream, decompressed, static_cast<uint32_t>(original.size()), false, true);
    REQUIRE(status.is_ok());
    REQUIRE_EQ(decompressed.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(decompressed[i], original[i]);
    }
}

TEST_CASE(ItCompression_ErrorHandlingTruncated) {
    // 8-bit truncated header
    std::vector<uint8_t> bad_data = {0x05}; // Only 1 byte for length
    tracker::io::MemoryInputStream s1(bad_data.data(), bad_data.size());
    std::vector<int8_t> out8;
    auto st1 = tracker::it::decompress_it_sample_8(s1, out8, 10, false, false);
    REQUIRE(!st1.is_ok());

    // 8-bit truncated body
    std::vector<uint8_t> bad_data2 = {0x10, 0x00, 0x01, 0x02}; // Declares 16 bytes, gives 2
    tracker::io::MemoryInputStream s2(bad_data2.data(), bad_data2.size());
    auto st2 = tracker::it::decompress_it_sample_8(s2, out8, 10, false, false);
    REQUIRE(!st2.is_ok());

    // 16-bit truncated header
    std::vector<int16_t> out16;
    tracker::io::MemoryInputStream s3(bad_data.data(), bad_data.size());
    auto st3 = tracker::it::decompress_it_sample_16(s3, out16, 10, false, false);
    REQUIRE(!st3.is_ok());
}
