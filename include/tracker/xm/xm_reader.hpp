#pragma once
#include <tracker/model.hpp>
#include <tracker/types.hpp>
#include <tracker/io/stream.hpp>
#include <string>

namespace tracker::xm {

class XmReader {
public:
    static Result<Song> load(io::InputStream& stream);
    static Result<Song> load_from_memory(const uint8_t* data, size_t size);
    static Result<Song> load_from_file(const std::string& path);
};

} // namespace tracker::xm
