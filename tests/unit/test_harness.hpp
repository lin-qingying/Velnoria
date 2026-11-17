#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "lumenvox/core/error.hpp"

// Minimal test harness: no third party dependency, no discovery magic beyond a
// static registrar. Keeping it tiny matters because the differential tests will
// grow their own runner and only share this for plain unit cases.

namespace lvtest {

struct Case {
    std::string name;
    std::function<void()> fn;
};

std::vector<Case>& registry();

struct Registrar {
    Registrar(const char* name, std::function<void()> fn);
};

class Failure : public std::runtime_error {
public:
    Failure(std::string message, const char* file, int line)
        : std::runtime_error(std::move(message)), file_(file), line_(line) {}

    [[nodiscard]] const char* file() const noexcept { return file_; }
    [[nodiscard]] int line() const noexcept { return line_; }

private:
    const char* file_;
    int line_;
};

int run_all(int argc, char** argv);

void fail(const std::string& message, const char* file, int line);

template <typename A, typename B>
void check_equal(const A& actual, const B& expected, const char* actual_expr,
                 const char* expected_expr, const char* file, int line) {
    if (!(actual == expected)) {
        fail(std::string(actual_expr) + " != " + expected_expr, file, line);
    }
}

}  // namespace lvtest

#define LV_TEST(name)                                                       \
    static void name();                                                     \
    static ::lvtest::Registrar lv_registrar_##name(#name, name);            \
    static void name()

#define LV_REQUIRE(cond)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ::lvtest::fail("requirement failed: " #cond, __FILE__, __LINE__); \
        }                                                                   \
    } while (false)

#define LV_CHECK_EQ(actual, expected)                                       \
    ::lvtest::check_equal((actual), (expected), #actual, #expected, __FILE__, __LINE__)

// Evaluates the expression exactly once: a decoder consumes its input, so
// re-evaluating it inside a macro would silently change the observed error.
#define LV_CHECK_ERR(expr, expected_code)                                   \
    do {                                                                    \
        auto&& lv_result_ = (expr);                                         \
        if (lv_result_.ok()) {                                              \
            ::lvtest::fail("expected error " #expected_code ", got success", \
                           __FILE__, __LINE__);                             \
        } else if (lv_result_.error().code != (expected_code)) {             \
            ::lvtest::fail(std::string("expected error " #expected_code ", got ") + \
                               std::string(::lumenvox::to_string(lv_result_.error().code)), \
                           __FILE__, __LINE__);                             \
        }                                                                   \
    } while (false)
