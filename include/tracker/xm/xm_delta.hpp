#pragma once
#include <vector>
#include <cstdint>

namespace tracker::xm {

std::vector<int8_t> decode_delta_8(const std::vector<int8_t>& delta_data);
std::vector<int8_t> encode_delta_8(const std::vector<int8_t>& pcm_data);

std::vector<int16_t> decode_delta_16(const std::vector<int16_t>& delta_data);
std::vector<int16_t> encode_delta_16(const std::vector<int16_t>& pcm_data);

} // namespace tracker::xm
