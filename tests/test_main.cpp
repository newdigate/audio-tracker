#include "test_main.hpp"

int main() {
    auto& tests = tracker::test::get_registry();
    std::cout << "Running " << tests.size() << " tests..." << std::endl;
    int passed = 0;
    for (const auto& test : tests) {
        std::cout << "  [RUN] " << test.name << std::endl;
        test.func();
        std::cout << "  [PASS] " << test.name << std::endl;
        passed++;
    }
    std::cout << "All " << passed << " tests passed successfully!" << std::endl;
    return 0;
}
