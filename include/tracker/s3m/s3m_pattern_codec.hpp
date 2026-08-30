#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>

namespace tracker::s3m {

Status unpack_pattern(io::InputStream& stream, Pattern& out_pat, uint16_t num_channels);
Status pack_pattern(const Pattern& pat, io::OutputStream& stream);

} // namespace tracker::s3m
