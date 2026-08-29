#include <tracker/xm/xm_delta.hpp>

namespace tracker::xm {

std::vector<int8_t> decode_delta_8(const std::vector<int8_t>& delta_data) {
    std::vector<int8_t> pcm(delta_data.size());
    int8_t current = 0;
    for (size_t i = 0; i < delta_data.size(); ++i) {
        current = static_cast<int8_t>(current + delta_data[i]);
        pcm[i] = current;
    }
    return pcm;
}

std::vector<int8_t> encode_delta_8(const std::vector<int8_t>& pcm_data) {
    std::vector<int8_t> delta(pcm_data.size());
    int8_t last = 0;
    for (size_t i = 0; i < pcm_data.size(); ++i) {
        delta[i] = static_cast<int8_t>(pcm_data[i] - last);
        last = pcm_data[i];
    }
    return delta;
}

std::vector<int16_t> decode_delta_16(const std::vector<int16_t>& delta_data) {
    std::vector<int16_t> pcm(delta_data.size());
    int16_t current = 0;
    for (size_t i = 0; i < delta_data.size(); ++i) {
        current = static_cast<int16_t>(current + delta_data[i]);
        pcm[i] = current;
    }
    return pcm;
}

std::vector<int16_t> encode_delta_16(const std::vector<int16_t>& pcm_data) {
    std::vector<int16_t> delta(pcm_data.size());
    int16_t last = 0;
    for (size_t i = 0; i < pcm_data.size(); ++i) {
        delta[i] = static_cast<int16_t>(pcm_data[i] - last);
        last = pcm_data[i];
    }
    return delta;
}

} // namespace tracker::xm
