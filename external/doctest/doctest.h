#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace doctest {

struct TestFailure : public std::exception {
    explicit TestFailure(std::string message) : message(std::move(message)) {}
    const char* what() const noexcept override { return message.c_str(); }
    std::string message;
};

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Register {
    Register(const char* name, std::function<void()> func) {
        registry().push_back(TestCase{name, std::move(func)});
    }
};

inline void fail(const char* expr, const char* file, int line) {
    std::ostringstream oss;
    oss << file << ':' << line << " CHECK(" << expr << ") failed";
    throw TestFailure(oss.str());
}

inline void fail_message(const char* expr, const char* file, int line, const std::string& lhs, const std::string& rhs) {
    std::ostringstream oss;
    oss << file << ':' << line << " CHECK_EQ(" << expr << ") failed (" << lhs << " vs " << rhs << ')';
    throw TestFailure(oss.str());
}

class Approx {
public:
    explicit Approx(double value) : value_(value), epsilon_(1e-6) {}
    Approx& epsilon(double eps) { epsilon_ = eps; return *this; }
    
    friend bool operator==(double lhs, const Approx& rhs) {
        double diff = lhs - rhs.value_;
        if (diff < 0) diff = -diff;
        return diff <= rhs.epsilon_;
    }
    friend bool operator==(const Approx& lhs, double rhs) { return rhs == lhs; }
    friend bool operator!=(double lhs, const Approx& rhs) { return !(lhs == rhs); }
    friend bool operator!=(const Approx& lhs, double rhs) { return !(lhs == rhs); }
    
    friend std::ostream& operator<<(std::ostream& os, const Approx& a) {
        return os << "Approx(" << a.value_ << ")";
    }
    
private:
    double value_;
    double epsilon_;
};

}  // namespace doctest

#define DOCTEST_JOIN_IMPL(s1, s2) s1##s2
#define DOCTEST_JOIN(s1, s2) DOCTEST_JOIN_IMPL(s1, s2)

#define TEST_CASE(name)                                                                                           \
    static void DOCTEST_JOIN(test_case_func_, __LINE__)();                                                        \
    static ::doctest::Register DOCTEST_JOIN(test_case_reg_, __LINE__)(name, DOCTEST_JOIN(test_case_func_, __LINE__)); \
    static void DOCTEST_JOIN(test_case_func_, __LINE__)()

#define CHECK(expr)                                                                                               \
    do {                                                                                                          \
        if (!(expr)) {                                                                                            \
            ::doctest::fail(#expr, __FILE__, __LINE__);                                                           \
        }                                                                                                         \
    } while (false)

#define CHECK_FALSE(expr) CHECK(!(expr))

#define CHECK_EQ(lhs, rhs)                                                                                        \
    do {                                                                                                          \
        if (!((lhs) == (rhs))) {                                                                                  \
            std::ostringstream oss_lhs;                                                                           \
            std::ostringstream oss_rhs;                                                                           \
            oss_lhs << (lhs);                                                                                     \
            oss_rhs << (rhs);                                                                                     \
            ::doctest::fail_message(#lhs " == " #rhs, __FILE__, __LINE__, oss_lhs.str(), oss_rhs.str());         \
        }                                                                                                         \
    } while (false)

#define REQUIRE(expr) CHECK(expr)
#define REQUIRE_FALSE(expr) CHECK_FALSE(expr)
#define REQUIRE_EQ(lhs, rhs) CHECK_EQ(lhs, rhs)

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
int main() {
    int failed = 0;
    for (const auto& test : ::doctest::registry()) {
        try {
            test.func();
            std::cout << "[pass] " << test.name << '\n';
        } catch (const ::doctest::TestFailure& failure) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - " << failure.what() << '\n';
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - unexpected exception: " << ex.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - unknown exception" << '\n';
        }
    }
    if (failed != 0) {
        std::cerr << failed << " test(s) failed" << '\n';
        return 1;
    }
    std::cout << "All tests passed" << '\n';
    return 0;
}
#endif
