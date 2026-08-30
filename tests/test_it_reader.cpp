#include "test_main.hpp"
#include <tracker/it/it_reader.hpp>
#include <tracker/it/it_types.hpp>
#include <tracker/it/it_compression.hpp>
#include <tracker/it/it_pattern_codec.hpp>
#include <tracker/io/memory_stream.hpp>
#include <tracker/io/file_stream.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
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
                    writer.write_bits(1U << (width - 1), width);
                    uint32_t target_width = 9;
                    uint32_t extra = (target_width >= width) ? (target_width - 2) : (target_width - 1);
                    writer.write_bits(extra, 3);
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
                    writer.write_bits(0x10000 | (5 - 1), 17);
                    width = 5;
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
                    writer.write_bits(1U << (width - 1), width);
                    uint32_t target_width = 17;
                    uint32_t extra = (target_width >= width) ? (target_width - 2) : (target_width - 1);
                    writer.write_bits(extra, 4);
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

TEST_CASE(ItReader_InvalidHeader) {
    std::vector<uint8_t> short_data(50, 0);
    auto res = tracker::it::ItReader::load_from_memory(short_data.data(), short_data.size());
    REQUIRE(!res.is_ok());
    REQUIRE_EQ(res.status().code, tracker::ErrorCode::InvalidSignature);

    // Null pointer
    auto res_null = tracker::it::ItReader::load_from_memory(nullptr, 0);
    REQUIRE(!res_null.is_ok());
    REQUIRE_EQ(res_null.status().code, tracker::ErrorCode::InvalidSignature);

    // 192 bytes with wrong magic
    std::vector<uint8_t> wrong_magic(192, 0);
    wrong_magic[0] = 'X'; wrong_magic[1] = 'X'; wrong_magic[2] = 'X'; wrong_magic[3] = 'X';
    auto res_magic = tracker::it::ItReader::load_from_memory(wrong_magic.data(), wrong_magic.size());
    REQUIRE(!res_magic.is_ok());
    REQUIRE_EQ(res_magic.status().code, tracker::ErrorCode::InvalidSignature);
}

TEST_CASE(ItReader_MinimalSong) {
    tracker::io::MemoryOutputStream out;
    // Magic: "IMPM"
    out.write("IMPM", 4);
    // Song name: 26 bytes
    std::string name = "Minimal IT Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0x0004); // phighlight
    out.write_u16_le(2);      // ordnum = 2
    out.write_u16_le(0);      // insnum = 0
    out.write_u16_le(0);      // smpnum = 0
    out.write_u16_le(0);      // patnum = 0
    out.write_u16_le(0x0214); // cwt_vwt = IT 2.14
    out.write_u16_le(0x0200); // cmwt = IT 2.00
    out.write_u16_le(tracker::it::IT_SONG_LINEAR_SLIDES); // flags: linear slides
    out.write_u16_le(0);      // special
    out.write_u8(128);        // global_vol
    out.write_u8(48);         // mix_vol
    out.write_u8(6);          // initial_speed
    out.write_u8(125);        // initial_tempo
    out.write_u8(128);        // pan_separation
    out.write_u8(0);          // pitch_wheel
    out.write_u16_le(0);      // msg_length
    out.write_u32_le(0);      // msg_offset
    out.write_u32_le(0);      // reserved

    // Channel pan (64 bytes): first 4 enabled (pan 32 = center), others 128 (disabled)
    for (size_t c = 0; c < 4; ++c) out.write_u8(32);
    for (size_t c = 4; c < 64; ++c) out.write_u8(128);

    // Channel vol (64 bytes): 64 for all
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    // Orders (2 bytes)
    out.write_u8(0);
    out.write_u8(255); // End of song

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.name, "Minimal IT Song");
    REQUIRE_EQ(song.tracker_name, "Impulse Tracker");
    REQUIRE_EQ(song.version, 0x0214);
    REQUIRE(song.linear_frequency);
    REQUIRE_EQ(song.global_volume, 128);
    REQUIRE_EQ(song.mix_volume, 48);
    REQUIRE_EQ(song.default_speed, 6);
    REQUIRE_EQ(song.default_bpm, 125);
    REQUIRE_EQ(song.pan_separation, 128);
    REQUIRE_EQ(song.num_channels, 4);
    REQUIRE_EQ(song.order_table.size(), 2);
    REQUIRE_EQ(song.order_table[0], 0);
    REQUIRE_EQ(song.order_table[1], 255);
}

TEST_CASE(ItReader_SongMessage) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Message Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);      // phighlight
    out.write_u16_le(1);      // ordnum = 1
    out.write_u16_le(0);      // insnum = 0
    out.write_u16_le(0);      // smpnum = 0
    out.write_u16_le(0);      // patnum = 0
    out.write_u16_le(0x0214); // cwt_vwt
    out.write_u16_le(0x0200); // cmwt
    out.write_u16_le(0);      // flags
    out.write_u16_le(tracker::it::IT_SPECIAL_MESSAGE); // special: has message
    out.write_u8(128);        // global_vol
    out.write_u8(48);         // mix_vol
    out.write_u8(6);          // initial_speed
    out.write_u8(125);        // initial_tempo
    out.write_u8(128);        // pan_sep
    out.write_u8(0);          // pitch_wheel

    std::string test_msg = "Hello from Impulse Tracker embedded message!\r\nLine 2.";
    uint16_t msg_len = static_cast<uint16_t>(test_msg.size());
    uint32_t msg_offset = 192 + 1; // Immediately after 1 order byte

    out.write_u16_le(msg_len);
    out.write_u32_le(msg_offset);
    out.write_u32_le(0);      // reserved

    // Channel pan (64 bytes)
    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    // Channel vol (64 bytes)
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    // Order 0
    out.write_u8(0);

    // Message data
    out.write(test_msg.data(), test_msg.size());

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());
    REQUIRE_EQ(res.value().message, test_msg);
}

TEST_CASE(ItReader_SampleMode_8BitUncompressed) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Sample Mode 8Bit";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);      // phighlight
    out.write_u16_le(1);      // ordnum = 1
    out.write_u16_le(0);      // insnum = 0 (Sample Mode)
    out.write_u16_le(1);      // smpnum = 1
    out.write_u16_le(0);      // patnum = 0
    out.write_u16_le(0x0214); // cwt_vwt
    out.write_u16_le(0x0200); // cmwt
    out.write_u16_le(0);      // flags
    out.write_u16_le(0);      // special
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    out.write_u8(0); // Order 0

    // Sample offset table (1 sample = 4 bytes)
    uint32_t smp_hdr_offset = 192 + 1 + 4;
    out.write_u32_le(smp_hdr_offset);

    // Sample Header (80 bytes)
    out.write("IMPS", 4);
    out.write("TESTSMP .WAV", 12);
    out.write_u8(0);  // zero
    out.write_u8(50); // global_vol
    // flags: exists | loop
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_LOOP);
    out.write_u8(45); // default_vol
    std::string sname = "Bass Wave";
    out.write(sname.data(), sname.size());
    for (size_t i = sname.size(); i < 26; ++i) out.write_u8(0);

    out.write_u8(tracker::it::IT_CONVERT_SIGNED); // convert_flags: signed PCM
    out.write_u8(0x80 | 16); // default_pan: enabled (bit 7) + pan 16 (16*4 = 64)
    out.write_u32_le(8);     // length = 8 frames
    out.write_u32_le(2);     // loop_start = 2
    out.write_u32_le(6);     // loop_end = 6 -> loop_len = 4
    out.write_u32_le(16726); // c5_speed = 16726 Hz
    out.write_u32_le(0);     // sus_loop_start
    out.write_u32_le(0);     // sus_loop_end

    uint32_t smp_data_offset = smp_hdr_offset + 80;
    out.write_u32_le(smp_data_offset); // sample_pointer
    out.write_u8(10); // vibrato_speed
    out.write_u8(20); // vibrato_depth
    out.write_u8(30); // vibrato_rate
    out.write_u8(1);  // vibrato_wave (ramp)

    // Audio data (8 bytes signed PCM)
    std::vector<int8_t> pcm = {0, 30, 60, 30, 0, -30, -60, -30};
    for (int8_t b : pcm) out.write_i8(b);

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    const auto& inst = song.instruments[0];
    REQUIRE_EQ(inst.name, "Bass Wave");
    REQUIRE_EQ(inst.samples.size(), 1);

    const auto& smp = inst.samples[0];
    REQUIRE_EQ(smp.name, "Bass Wave");
    REQUIRE_EQ(smp.global_volume, 50);
    REQUIRE_EQ(smp.volume, 45);
    REQUIRE_EQ(smp.panning, 64);
    REQUIRE_EQ(smp.c5_speed, 16726);
    REQUIRE_EQ(smp.length, 8);
    REQUIRE(smp.loop_type == tracker::LoopType::Forward);
    REQUIRE_EQ(smp.loop_start, 2);
    REQUIRE_EQ(smp.loop_length, 4);
    REQUIRE_EQ(smp.vibrato_sweep, 10);
    REQUIRE_EQ(smp.vibrato_depth, 20);
    REQUIRE_EQ(smp.vibrato_rate, 30);
    REQUIRE_EQ(smp.vibrato_type, 1);
    REQUIRE(!smp.is_16bit);
    REQUIRE_EQ(smp.data8.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_EQ(smp.data8[i], pcm[i]);
    }
}

TEST_CASE(ItReader_SampleMode_16BitUncompressed) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "16Bit Sample Test";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(1); // 1 sample
    out.write_u16_le(0);
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    out.write_u8(0); // Order 0

    uint32_t smp_hdr_offset = 192 + 1 + 4;
    out.write_u32_le(smp_hdr_offset);

    // Sample Header
    out.write("IMPS", 4);
    out.write("16BIT   .WAV", 12);
    out.write_u8(0);
    out.write_u8(64);
    // flags: exists | 16-bit | sustain loop | pingpong sustain
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_16BIT |
                 tracker::it::IT_SAMPLE_SUSTAIN_LOOP | tracker::it::IT_SAMPLE_PINGPONG_SUSTAIN);
    out.write_u8(64);
    std::string sname = "HiFi Lead";
    out.write(sname.data(), sname.size());
    for (size_t i = sname.size(); i < 26; ++i) out.write_u8(0);

    out.write_u8(tracker::it::IT_CONVERT_SIGNED);
    out.write_u8(0); // default_pan not enabled -> 128
    out.write_u32_le(4);     // length = 4 frames (8 bytes)
    out.write_u32_le(0);     // loop_start
    out.write_u32_le(0);     // loop_end
    out.write_u32_le(44100); // c5_speed
    out.write_u32_le(1);     // sus_loop_start = 1
    out.write_u32_le(3);     // sus_loop_end = 3 -> sus_loop_len = 2

    uint32_t smp_data_offset = smp_hdr_offset + 80;
    out.write_u32_le(smp_data_offset);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0);

    // Audio data (4 samples signed 16-bit LE)
    std::vector<int16_t> pcm16 = {1000, 2000, -2000, -1000};
    for (int16_t v : pcm16) out.write_i16_le(v);

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    const auto& smp = song.instruments[0].samples[0];
    REQUIRE(smp.is_16bit);
    REQUIRE_EQ(smp.length, 4);
    REQUIRE_EQ(smp.c5_speed, 44100);
    REQUIRE(smp.sustain_loop_type == tracker::LoopType::PingPong);
    REQUIRE_EQ(smp.sustain_loop_start, 1);
    REQUIRE_EQ(smp.sustain_loop_length, 2);
    REQUIRE_EQ(smp.data16.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_EQ(smp.data16[i], pcm16[i]);
    }
}

TEST_CASE(ItReader_CompressedSamples_8BitAnd16Bit) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Compressed Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0); // Sample mode
    out.write_u16_le(2); // 2 samples
    out.write_u16_le(0);
    out.write_u16_le(0x0215); // IT 2.15!
    out.write_u16_le(0x0215);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);
    out.write_u8(0); // Order 0

    // Sample 1: 8-bit compressed double-delta (IT 2.15)
    std::vector<int8_t> pcm8(100);
    for (size_t i = 0; i < pcm8.size(); ++i) pcm8[i] = static_cast<int8_t>(i % 50 - 25);
    auto comp8 = compress_it_sample_8(pcm8, true, false);

    // Sample 2: 16-bit compressed double-delta (IT 2.15)
    std::vector<int16_t> pcm16(100);
    for (size_t i = 0; i < pcm16.size(); ++i) pcm16[i] = static_cast<int16_t>(i * 100 - 5000);
    auto comp16 = compress_it_sample_16(pcm16, true, false);

    uint32_t smp1_hdr_offset = 192 + 1 + 8; // ord (1) + 2 smp ptrs (8)
    uint32_t smp2_hdr_offset = smp1_hdr_offset + 80;
    uint32_t smp1_data_offset = smp2_hdr_offset + 80;
    uint32_t smp2_data_offset = smp1_data_offset + static_cast<uint32_t>(comp8.size());

    out.write_u32_le(smp1_hdr_offset);
    out.write_u32_le(smp2_hdr_offset);

    // Sample 1 Header (8-bit compressed)
    out.write("IMPS", 4);
    out.write("SMP8COMP.WAV", 12);
    out.write_u8(0);
    out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_COMPRESSED); // 8-bit compressed
    out.write_u8(64);
    std::string s1_name = "8Bit Compressed";
    out.write(s1_name.data(), s1_name.size());
    for (size_t i = s1_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(tracker::it::IT_CONVERT_SIGNED);
    out.write_u8(0);
    out.write_u32_le(static_cast<uint32_t>(pcm8.size()));
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp1_data_offset);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // Sample 2 Header (16-bit compressed)
    out.write("IMPS", 4);
    out.write("SMP16CMP.WAV", 12);
    out.write_u8(0);
    out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_16BIT | tracker::it::IT_SAMPLE_COMPRESSED);
    out.write_u8(64);
    std::string s2_name = "16Bit Compressed";
    out.write(s2_name.data(), s2_name.size());
    for (size_t i = s2_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(tracker::it::IT_CONVERT_SIGNED);
    out.write_u8(0);
    out.write_u32_le(static_cast<uint32_t>(pcm16.size()));
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp2_data_offset);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // Data 1
    out.write(comp8.data(), comp8.size());
    // Data 2
    out.write(comp16.data(), comp16.size());

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 2);

    const auto& s1 = song.instruments[0].samples[0];
    REQUIRE(!s1.is_16bit);
    REQUIRE_EQ(s1.data8.size(), pcm8.size());
    for (size_t i = 0; i < pcm8.size(); ++i) {
        REQUIRE_EQ(s1.data8[i], pcm8[i]);
    }

    const auto& s2 = song.instruments[1].samples[0];
    REQUIRE(s2.is_16bit);
    REQUIRE_EQ(s2.data16.size(), pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        REQUIRE_EQ(s2.data16[i], pcm16[i]);
    }
}

TEST_CASE(ItReader_InstrumentMode) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Instrument Mode Test";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(1); // 1 instrument
    out.write_u16_le(1); // 1 sample
    out.write_u16_le(0);
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(tracker::it::IT_SONG_INSTRUMENTS); // Use Instruments flag!
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    out.write_u8(0); // Order 0

    uint32_t inst_offset = 192 + 1 + 4 + 4; // after ord (1) + inst_ptr (4) + smp_ptr (4)
    uint32_t smp_offset = inst_offset + 554;

    out.write_u32_le(inst_offset);
    out.write_u32_le(smp_offset);

    // Instrument Header (554 bytes)
    out.write("IMPI", 4);
    out.write("INSTFILE.ITI", 12);
    out.write_u8(0); // zero
    out.write_u8(static_cast<uint8_t>(tracker::NewNoteAction::NoteOff)); // nna = 2
    out.write_u8(static_cast<uint8_t>(tracker::DuplicateCheckType::Sample)); // dct = 2
    out.write_u8(static_cast<uint8_t>(tracker::DuplicateCheckAction::NoteFade)); // dca = 2
    out.write_u16_le(512); // fadeout
    out.write_u8(0); // pitchpan_sep
    out.write_u8(60); // pitchpan_center
    out.write_u8(100); // global_vol
    out.write_u8(32); // default_pan = 32 -> 128
    out.write_u16_le(0); // random_var
    out.write_u16_le(0x0214);
    out.write_u8(1); // num_samples
    out.write_u8(0); // reserved
    std::string iname = "Complex Instrument";
    out.write(iname.data(), iname.size());
    for (size_t i = iname.size(); i < 26; ++i) out.write_u8(0);
    out.write_u16_le(0); // initial_filter
    out.write_u32_le(0); // midi_settings

    // Keyboard map: 120 pairs
    for (size_t k = 0; k < 120; ++k) {
        out.write_u8(static_cast<uint8_t>(k)); // note
        out.write_u8(1); // sample 1
    }

    // Volume envelope (82 bytes): enabled | loop | sustain (flags=7), 2 nodes
    out.write_u8(7); // flags: env_on | loop_on | sus_on
    out.write_u8(2); // num_nodes = 2
    out.write_u8(0); // loop_start
    out.write_u8(1); // loop_end
    out.write_u8(0); // sus_start
    out.write_u8(1); // sus_end
    // Node 0: val=64, tick=0
    out.write_u8(64); out.write_u16_le(0);
    // Node 1: val=0, tick=50
    out.write_u8(0); out.write_u16_le(50);
    // Remaining 23 dummy nodes (23 * 3 = 69 bytes)
    for (size_t k = 2; k < 25; ++k) { out.write_u8(0); out.write_u16_le(0); }
    out.write_u8(0); // reserved padding to 82 bytes

    // Pan envelope (82 bytes): disabled
    out.write_u8(0); // flags
    out.write_u8(0); // num_nodes
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);
    for (size_t k = 0; k < 25; ++k) { out.write_u8(0); out.write_u16_le(0); }
    out.write_u8(0);

    // Pitch envelope (82 bytes): enabled (flags=1), 1 node
    out.write_u8(1); // flags
    out.write_u8(1); // num_nodes = 1
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);
    out.write_u8(32); out.write_u16_le(10); // node 0: val 32, tick 10
    for (size_t k = 1; k < 25; ++k) { out.write_u8(0); out.write_u16_le(0); }
    out.write_u8(0);

    // Reserved (4 bytes)
    out.write_u32_le(0);

    // Sample Header (80 bytes)
    out.write("IMPS", 4);
    out.write("SMP1    .WAV", 12);
    out.write_u8(0);
    out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS);
    out.write_u8(64);
    std::string smp_name = "Inst Sample";
    out.write(smp_name.data(), smp_name.size());
    for (size_t i = smp_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(tracker::it::IT_CONVERT_SIGNED);
    out.write_u8(0);
    out.write_u32_le(2); // length = 2
    out.write_u32_le(0);
    out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0);
    out.write_u32_le(0);
    uint32_t smp_pcm_offset = smp_offset + 80;
    out.write_u32_le(smp_pcm_offset);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0);
    out.write_u8(0);

    // Sample data (2 bytes)
    out.write_i8(10);
    out.write_i8(-10);

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.instruments.size(), 1);
    const auto& inst = song.instruments[0];
    REQUIRE_EQ(inst.name, "Complex Instrument");
    REQUIRE_EQ(inst.filename, "INSTFILE.ITI");
    REQUIRE(inst.nna == tracker::NewNoteAction::NoteOff);
    REQUIRE(inst.dct == tracker::DuplicateCheckType::Sample);
    REQUIRE(inst.dca == tracker::DuplicateCheckAction::NoteFade);
    REQUIRE_EQ(inst.volume_fadeout, 512);
    REQUIRE_EQ(inst.global_volume, 100);
    REQUIRE_EQ(inst.default_panning, 128);

    REQUIRE(inst.volume_envelope.enabled);
    REQUIRE(inst.volume_envelope.loop_enabled);
    REQUIRE(inst.volume_envelope.sustain_enabled);
    REQUIRE_EQ(inst.volume_envelope.points.size(), 2);
    REQUIRE_EQ(inst.volume_envelope.points[0].tick, 0);
    REQUIRE_EQ(inst.volume_envelope.points[0].value, 64);
    REQUIRE_EQ(inst.volume_envelope.points[1].tick, 50);
    REQUIRE_EQ(inst.volume_envelope.points[1].value, 0);

    REQUIRE(!inst.panning_envelope.enabled);

    REQUIRE(inst.pitch_envelope.enabled);
    REQUIRE_EQ(inst.pitch_envelope.points.size(), 1);
    REQUIRE_EQ(inst.pitch_envelope.points[0].tick, 10);
    REQUIRE_EQ(inst.pitch_envelope.points[0].value, 32);

    REQUIRE_EQ(inst.samples.size(), 1);
    REQUIRE_EQ(inst.samples[0].name, "Inst Sample");
    REQUIRE_EQ(inst.samples[0].length, 2);
    REQUIRE_EQ(inst.samples[0].data8.size(), 2);
    REQUIRE_EQ(inst.samples[0].data8[0], 10);
    REQUIRE_EQ(inst.samples[0].data8[1], -10);
}

TEST_CASE(ItReader_PatternData) {
    tracker::Song song_ref;
    song_ref.name = "Pattern Test Song";
    song_ref.num_channels = 4;
    song_ref.order_table = {0};

    tracker::Pattern pat(64, 4);
    // Row 0, Ch 0: Note 49 (C-4), Inst 1, Vol 0x50 (64), Effect SetSpeed 6
    auto& c0 = pat.get_cell(0, 0);
    c0.note = 49;
    c0.instrument = 1;
    c0.volume = 0x50;
    c0.effect_type = 1;
    c0.effect_param = 6;

    // Row 2, Ch 3: Note Cut (253), Inst 2
    auto& c1 = pat.get_cell(2, 3);
    c1.note = tracker::it::IT_NOTE_CUT;
    c1.instrument = 2;

    tracker::io::MemoryOutputStream pat_stream;
    REQUIRE(tracker::it::pack_pattern(pat, pat_stream).is_ok());

    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = song_ref.name;
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(1); // 1 pattern
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    out.write_u8(0); // Order 0

    uint32_t pat_offset = 192 + 1 + 4; // after ord (1) + pat_ptr (4)
    out.write_u32_le(pat_offset);

    // Pattern Header (8 bytes): length (2), rows (2), reserved (4)
    out.write_u16_le(static_cast<uint16_t>(pat_stream.data().size()));
    out.write_u16_le(64); // 64 rows
    out.write_u32_le(0);  // reserved
    out.write(pat_stream.data().data(), pat_stream.data().size());

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& loaded_song = res.value();
    REQUIRE_EQ(loaded_song.num_channels, 4);
    REQUIRE_EQ(loaded_song.patterns.size(), 1);
    REQUIRE_EQ(loaded_song.patterns[0].num_rows, 64);

    const auto& lc0 = loaded_song.patterns[0].get_cell(0, 0);
    REQUIRE_EQ(lc0.note, 49);
    REQUIRE_EQ(lc0.instrument, 1);
    REQUIRE_EQ(lc0.volume, 0x50);
    REQUIRE_EQ(lc0.effect_type, 1);
    REQUIRE_EQ(lc0.effect_param, 6);

    const auto& lc1 = loaded_song.patterns[0].get_cell(2, 3);
    REQUIRE_EQ(lc1.note, tracker::it::IT_NOTE_CUT);
    REQUIRE_EQ(lc1.instrument, 2);
}

TEST_CASE(ItReader_LoadFromFile) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "File Load Test";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);
    out.write_u8(0);

    const std::string tmp_path = "test_it_reader_temp.it";
    FILE* fp = fopen(tmp_path.c_str(), "wb");
    REQUIRE(fp != nullptr);
    fwrite(out.data().data(), 1, out.data().size(), fp);
    fclose(fp);

    auto res = tracker::it::ItReader::load_from_file(tmp_path);
    std::remove(tmp_path.c_str());

    REQUIRE(res.is_ok());
    REQUIRE_EQ(res.value().name, "File Load Test");

    // Test non-existent file
    auto res_missing = tracker::it::ItReader::load_from_file("non_existent_file_12345.it");
    REQUIRE(!res_missing.is_ok());
    REQUIRE_EQ(res_missing.status().code, tracker::ErrorCode::IoError);
}

TEST_CASE(ItReader_UncompressedUnsignedPcm) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Unsigned PCM Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(2); // 2 samples: 8-bit unsigned, 16-bit unsigned
    out.write_u16_le(0);
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);
    out.write_u8(0);

    uint32_t smp1_hdr = 192 + 1 + 8;
    uint32_t smp2_hdr = smp1_hdr + 80;
    uint32_t smp1_data = smp2_hdr + 80;
    uint32_t smp2_data = smp1_data + 4;

    out.write_u32_le(smp1_hdr);
    out.write_u32_le(smp2_hdr);

    // Sample 1: 8-bit unsigned PCM
    out.write("IMPS", 4);
    out.write("SMP8U   .WAV", 12);
    out.write_u8(0); out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS);
    out.write_u8(64);
    std::string s1_name = "8Bit Unsigned";
    out.write(s1_name.data(), s1_name.size());
    for (size_t i = s1_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(0); // convert_flags = 0 -> unsigned!
    out.write_u8(0);
    out.write_u32_le(4);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp1_data);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // Sample 2: 16-bit unsigned PCM
    out.write("IMPS", 4);
    out.write("SMP16U  .WAV", 12);
    out.write_u8(0); out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_16BIT);
    out.write_u8(64);
    std::string s2_name = "16Bit Unsigned";
    out.write(s2_name.data(), s2_name.size());
    for (size_t i = s2_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(0); // unsigned!
    out.write_u8(0);
    out.write_u32_le(4);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp2_data);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // 8-bit unsigned PCM data: 128 (0), 255 (127), 0 (-128), 128 (0)
    out.write_u8(128); out.write_u8(255); out.write_u8(0); out.write_u8(128);

    // 16-bit unsigned PCM data: 32768 (0), 65535 (32767), 0 (-32768), 32768 (0)
    out.write_u16_le(32768); out.write_u16_le(65535); out.write_u16_le(0); out.write_u16_le(32768);

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& s1 = res.value().instruments[0].samples[0];
    REQUIRE_EQ(s1.data8[0], 0);
    REQUIRE_EQ(s1.data8[1], 127);
    REQUIRE_EQ(s1.data8[2], -128);
    REQUIRE_EQ(s1.data8[3], 0);

    const auto& s2 = res.value().instruments[1].samples[0];
    REQUIRE_EQ(s2.data16[0], 0);
    REQUIRE_EQ(s2.data16[1], 32767);
    REQUIRE_EQ(s2.data16[2], -32768);
    REQUIRE_EQ(s2.data16[3], 0);
}

TEST_CASE(ItReader_UncompressedDeltaPcm) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Delta PCM Song";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(0);
    out.write_u16_le(2); // 2 samples: 8-bit delta, 16-bit delta
    out.write_u16_le(0);
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(0);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 64; ++c) out.write_u8(c < 4 ? 32 : 128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);
    out.write_u8(0);

    uint32_t smp1_hdr = 192 + 1 + 8;
    uint32_t smp2_hdr = smp1_hdr + 80;
    uint32_t smp1_data = smp2_hdr + 80;
    uint32_t smp2_data = smp1_data + 4;

    out.write_u32_le(smp1_hdr);
    out.write_u32_le(smp2_hdr);

    // Sample 1: 8-bit delta
    out.write("IMPS", 4);
    out.write("SMP8D   .WAV", 12);
    out.write_u8(0); out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS);
    out.write_u8(64);
    std::string s1_name = "8Bit Delta";
    out.write(s1_name.data(), s1_name.size());
    for (size_t i = s1_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(tracker::it::IT_CONVERT_DELTA); // delta!
    out.write_u8(0);
    out.write_u32_le(4);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp1_data);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // Sample 2: 16-bit delta
    out.write("IMPS", 4);
    out.write("SMP16D  .WAV", 12);
    out.write_u8(0); out.write_u8(64);
    out.write_u8(tracker::it::IT_SAMPLE_EXISTS | tracker::it::IT_SAMPLE_16BIT);
    out.write_u8(64);
    std::string s2_name = "16Bit Delta";
    out.write(s2_name.data(), s2_name.size());
    for (size_t i = s2_name.size(); i < 26; ++i) out.write_u8(0);
    out.write_u8(tracker::it::IT_CONVERT_DELTA); // delta!
    out.write_u8(0);
    out.write_u32_le(4);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(8363);
    out.write_u32_le(0); out.write_u32_le(0);
    out.write_u32_le(smp2_data);
    out.write_u8(0); out.write_u8(0); out.write_u8(0); out.write_u8(0);

    // 8-bit deltas: +10, +20, -5, -25 -> samples: 10, 30, 25, 0
    out.write_i8(10); out.write_i8(20); out.write_i8(-5); out.write_i8(-25);

    // 16-bit deltas: +100, +200, -50, -250 -> samples: 100, 300, 250, 0
    out.write_i16_le(100); out.write_i16_le(200); out.write_i16_le(-50); out.write_i16_le(-250);

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& s1 = res.value().instruments[0].samples[0];
    REQUIRE_EQ(s1.data8[0], 10);
    REQUIRE_EQ(s1.data8[1], 30);
    REQUIRE_EQ(s1.data8[2], 25);
    REQUIRE_EQ(s1.data8[3], 0);

    const auto& s2 = res.value().instruments[1].samples[0];
    REQUIRE_EQ(s2.data16[0], 100);
    REQUIRE_EQ(s2.data16[1], 300);
    REQUIRE_EQ(s2.data16[2], 250);
    REQUIRE_EQ(s2.data16[3], 0);
}

TEST_CASE(ItReader_EmptyOffsetsAndDefaultBlankPattern) {
    tracker::io::MemoryOutputStream out;
    out.write("IMPM", 4);
    std::string name = "Empty Offsets";
    out.write(name.data(), name.size());
    for (size_t i = name.size(); i < 26; ++i) out.write_u8(0);

    out.write_u16_le(0);
    out.write_u16_le(1);
    out.write_u16_le(1); // 1 instrument (offset 0)
    out.write_u16_le(1); // 1 sample (offset 0)
    out.write_u16_le(1); // 1 pattern (offset 0)
    out.write_u16_le(0x0214);
    out.write_u16_le(0x0200);
    out.write_u16_le(tracker::it::IT_SONG_INSTRUMENTS);
    out.write_u16_le(0);
    out.write_u8(128);
    out.write_u8(48);
    out.write_u8(6);
    out.write_u8(125);
    out.write_u8(128);
    out.write_u8(0);
    out.write_u16_le(0);
    out.write_u32_le(0);
    out.write_u32_le(0);

    for (size_t c = 0; c < 16; ++c) out.write_u8(32); // 16 channels enabled
    for (size_t c = 16; c < 64; ++c) out.write_u8(128);
    for (size_t c = 0; c < 64; ++c) out.write_u8(64);

    out.write_u8(0); // Order 0

    // Pointer tables all 0 (empty)
    out.write_u32_le(0); // inst_ptr = 0
    out.write_u32_le(0); // smp_ptr = 0
    out.write_u32_le(0); // pat_ptr = 0

    auto res = tracker::it::ItReader::load_from_memory(out.data().data(), out.data().size());
    REQUIRE(res.is_ok());

    const auto& song = res.value();
    REQUIRE_EQ(song.num_channels, 16);
    REQUIRE_EQ(song.patterns.size(), 1);
    REQUIRE_EQ(song.patterns[0].num_rows, 64);
    REQUIRE_EQ(song.patterns[0].num_channels, 16);
    REQUIRE(song.patterns[0].is_all_empty());
}
