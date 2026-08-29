#pragma once
#include <cstdint>
#include <cstddef>

namespace tracker::xm {

constexpr const char* XM_SIGNATURE = "Extended Module: ";
constexpr size_t XM_SIGNATURE_LEN = 17;
constexpr uint8_t XM_EOF_BYTE = 0x1A;
constexpr uint16_t XM_VERSION_104 = 0x0104;

constexpr uint32_t XM_HEADER_SIZE_104 = 276;
constexpr uint32_t XM_PATTERN_HEADER_LEN = 9;
constexpr uint32_t XM_INST_HEADER_EMPTY_LEN = 29;
constexpr uint32_t XM_INST_HEADER_FULL_LEN = 263;
constexpr uint32_t XM_SAMPLE_HEADER_LEN = 40;

// Cell packing bitmasks
constexpr uint8_t XM_PACK_NOTE        = 0x01;
constexpr uint8_t XM_PACK_INSTRUMENT  = 0x02;
constexpr uint8_t XM_PACK_VOLUME      = 0x04;
constexpr uint8_t XM_PACK_EFFECT_TYPE = 0x08;
constexpr uint8_t XM_PACK_EFFECT_PARAM= 0x10;
constexpr uint8_t XM_PACK_FLAG        = 0x80;

// Sample flags
constexpr uint8_t XM_SAMPLE_LOOP_NONE     = 0x00;
constexpr uint8_t XM_SAMPLE_LOOP_FORWARD  = 0x01;
constexpr uint8_t XM_SAMPLE_LOOP_PINGPONG = 0x02;
constexpr uint8_t XM_SAMPLE_16BIT         = 0x10;

} // namespace tracker::xm
