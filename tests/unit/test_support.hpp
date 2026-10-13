#pragma once

// Tiny assertion helper so tests build without downloading a framework.
// CHECK records a failure and keeps going; return finishTests() from main().

#include <iostream>

namespace testing_support {

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline int& checkCount() {
    static int checks = 0;
    return checks;
}

}  // namespace testing_support

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++testing_support::checkCount();                                                    \
        if (!(cond)) {                                                                      \
            ++testing_support::failureCount();                                              \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond << "\n";   \
        }                                                                                   \
    } while (0)

#define CHECK_EQ(actual, expected)                                                          \
    do {                                                                                    \
        ++testing_support::checkCount();                                                    \
        const auto actual_ = (actual);                                                      \
        const auto expected_ = (expected);                                                  \
        if (!(actual_ == expected_)) {                                                      \
            ++testing_support::failureCount();                                              \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #actual       \
                      << " = " << actual_ << ", expected " << expected_ << "\n";            \
        }                                                                                   \
    } while (0)

inline int finishTests(const char* name) {
    std::cout << name << ": " << testing_support::checkCount() << " checks, "
              << testing_support::failureCount() << " failed\n";
    return testing_support::failureCount() == 0 ? 0 : 1;
}
