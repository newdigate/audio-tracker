#pragma once
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <vector>
#include <cstdint>

namespace tracker::it {

Status decompress_it_sample_8(io::InputStream& stream,
                              std::vector<int8_t>& out_data,
                              uint32_t length_frames,
                              bool is_it215,
                              bool is_delta);

Status decompress_it_sample_16(io::InputStream& stream,
                               std::vector<int16_t>& out_data,
                               uint32_t length_frames,
                               bool is_it215,
                               bool is_delta);

} // namespace tracker::it
