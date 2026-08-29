#include "test_main.hpp"
#include <tracker/types.hpp>

TEST_CASE(Types_StatusAndResult) {
    tracker::Status ok_status = tracker::Status::ok();
    REQUIRE(ok_status.is_ok());
    REQUIRE((bool)ok_status);

    tracker::Status err_status = tracker::Status::error(tracker::ErrorCode::CorruptHeader, "Header is corrupt");
    REQUIRE(!err_status.is_ok());
    REQUIRE_EQ(err_status.code, tracker::ErrorCode::CorruptHeader);
    REQUIRE_EQ(err_status.message, "Header is corrupt");

    tracker::Result<int> res(42);
    REQUIRE(res.is_ok());
    REQUIRE_EQ(res.value(), 42);

    tracker::Result<int> res_err(tracker::ErrorCode::UnexpectedEof, "EOF");
    REQUIRE(!res_err.is_ok());
    REQUIRE_EQ(res_err.status().code, tracker::ErrorCode::UnexpectedEof);
}
