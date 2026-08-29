#include "test_main.hpp"
#include <tracker/xm/xm_delta.hpp>
#include <tracker/xm/xm_types.hpp>

TEST_CASE(XmDelta_8BitRoundTrip) {
    std::vector<int8_t> original = {0, 10, 20, 15, -30, -128, 127, 0};
    auto encoded = tracker::xm::encode_delta_8(original);
    auto decoded = tracker::xm::decode_delta_8(encoded);
    REQUIRE_EQ(original.size(), decoded.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(original[i], decoded[i]);
    }
}

TEST_CASE(XmDelta_16BitRoundTrip) {
    std::vector<int16_t> original = {0, 1000, 30000, -32768, 32767, -100, 0};
    auto encoded = tracker::xm::encode_delta_16(original);
    auto decoded = tracker::xm::decode_delta_16(encoded);
    REQUIRE_EQ(original.size(), decoded.size());
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE_EQ(original[i], decoded[i]);
    }
}

TEST_CASE(XmDelta_EmptyAndSingleElement) {
    std::vector<int8_t> empty8;
    REQUIRE_EQ(tracker::xm::encode_delta_8(empty8).size(), 0u);
    REQUIRE_EQ(tracker::xm::decode_delta_8(empty8).size(), 0u);

    std::vector<int16_t> empty16;
    REQUIRE_EQ(tracker::xm::encode_delta_16(empty16).size(), 0u);
    REQUIRE_EQ(tracker::xm::decode_delta_16(empty16).size(), 0u);

    std::vector<int8_t> single8 = {42};
    auto enc8 = tracker::xm::encode_delta_8(single8);
    auto dec8 = tracker::xm::decode_delta_8(enc8);
    REQUIRE_EQ(dec8.size(), 1u);
    REQUIRE_EQ(dec8[0], 42);

    std::vector<int16_t> single16 = {-12345};
    auto enc16 = tracker::xm::encode_delta_16(single16);
    auto dec16 = tracker::xm::decode_delta_16(enc16);
    REQUIRE_EQ(dec16.size(), 1u);
    REQUIRE_EQ(dec16[0], -12345);
}

TEST_CASE(XmTypes_Constants) {
    REQUIRE_EQ(std::string(tracker::xm::XM_SIGNATURE), std::string("Extended Module: "));
    REQUIRE_EQ(tracker::xm::XM_SIGNATURE_LEN, 17u);
    REQUIRE_EQ(tracker::xm::XM_EOF_BYTE, 0x1A);
    REQUIRE_EQ(tracker::xm::XM_VERSION_104, 0x0104);
    REQUIRE_EQ(tracker::xm::XM_HEADER_SIZE_104, 276u);
    REQUIRE_EQ(tracker::xm::XM_PATTERN_HEADER_LEN, 9u);
    REQUIRE_EQ(tracker::xm::XM_INST_HEADER_EMPTY_LEN, 29u);
    REQUIRE_EQ(tracker::xm::XM_INST_HEADER_FULL_LEN, 263u);
    REQUIRE_EQ(tracker::xm::XM_SAMPLE_HEADER_LEN, 40u);
    REQUIRE_EQ(tracker::xm::XM_PACK_FLAG, 0x80);
    REQUIRE_EQ(tracker::xm::XM_SAMPLE_16BIT, 0x10);
}
