#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>

namespace tracker::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool register_test(const std::string& name, std::function<void()> func) {
    get_registry().push_back({name, std::move(func)});
    return true;
}

#define TEST_CASE(name) \
    static void test_func_##name(); \
    static const bool test_reg_##name = tracker::test::register_test(#name, test_func_##name); \
    static void test_func_##name()

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

#define REQUIRE_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "FAILED: " << #a << " == " << #b << " (got " << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

} // namespace tracker::test
