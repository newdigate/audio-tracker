#include <tracker/model.hpp>

namespace tracker {

Pattern::Pattern(uint16_t rows, uint16_t channels)
    : num_rows(rows), num_channels(channels), cells(static_cast<size_t>(rows) * channels) {}

Cell& Pattern::get_cell(uint16_t row, uint16_t channel) {
    return cells[static_cast<size_t>(row) * num_channels + channel];
}

const Cell& Pattern::get_cell(uint16_t row, uint16_t channel) const {
    return cells[static_cast<size_t>(row) * num_channels + channel];
}

bool Pattern::is_all_empty() const noexcept {
    for (const auto& cell : cells) {
        if (!cell.is_empty()) return false;
    }
    return true;
}

} // namespace tracker
