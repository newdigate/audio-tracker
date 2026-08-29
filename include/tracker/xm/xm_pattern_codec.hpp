#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <vector>
#include <cstdint>

namespace tracker::xm {

std::vector<uint8_t> pack_pattern(const Pattern& pattern);
Status unpack_pattern(const uint8_t* packed_data, size_t packed_size, Pattern& out_pattern);

} // namespace tracker::xm
