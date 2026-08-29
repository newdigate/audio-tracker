#pragma once
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

namespace tracker {

enum class ErrorCode {
    Ok = 0,
    InvalidSignature,
    UnsupportedVersion,
    CorruptHeader,
    CorruptPatternData,
    CorruptInstrumentData,
    CorruptSampleData,
    UnexpectedEof,
    InvalidChannelCount,
    IoError,
    WriteError
};

inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return os << "Ok";
        case ErrorCode::InvalidSignature: return os << "InvalidSignature";
        case ErrorCode::UnsupportedVersion: return os << "UnsupportedVersion";
        case ErrorCode::CorruptHeader: return os << "CorruptHeader";
        case ErrorCode::CorruptPatternData: return os << "CorruptPatternData";
        case ErrorCode::CorruptInstrumentData: return os << "CorruptInstrumentData";
        case ErrorCode::CorruptSampleData: return os << "CorruptSampleData";
        case ErrorCode::UnexpectedEof: return os << "UnexpectedEof";
        case ErrorCode::InvalidChannelCount: return os << "InvalidChannelCount";
        case ErrorCode::IoError: return os << "IoError";
        case ErrorCode::WriteError: return os << "WriteError";
        default: return os << "Unknown(" << static_cast<int>(code) << ")";
    }
}


struct Status {
    ErrorCode code{ErrorCode::Ok};
    std::string message;

    static Status ok() { return Status{ErrorCode::Ok, ""}; }
    static Status error(ErrorCode c, std::string msg) { return Status{c, std::move(msg)}; }

    bool is_ok() const noexcept { return code == ErrorCode::Ok; }
    explicit operator bool() const noexcept { return is_ok(); }
};

template <typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)), m_status(Status::ok()) {}
    Result(Status status) : m_status(std::move(status)) {}
    Result(ErrorCode code, std::string msg) : m_status(Status::error(code, std::move(msg))) {}

    bool is_ok() const noexcept { return m_status.is_ok(); }
    explicit operator bool() const noexcept { return is_ok(); }

    const Status& status() const noexcept { return m_status; }
    T& value() noexcept { return m_value; }
    const T& value() const noexcept { return m_value; }

private:
    T m_value{};
    Status m_status;
};

} // namespace tracker
