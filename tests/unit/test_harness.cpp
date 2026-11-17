#include "test_harness.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

namespace lvtest {
namespace {

int g_failures = 0;

bool matches_filter(const std::string& name, const std::string& filter) {
    return filter.empty() || name.find(filter) != std::string::npos;
}

}  // namespace

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

Registrar::Registrar(const char* name, std::function<void()> fn) {
    registry().push_back(Case{name, std::move(fn)});
}

void fail(const std::string& message, const char* file, int line) {
    throw Failure(message, file, line);
}

int run_all(int argc, char** argv) {
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        }
    }

    int executed = 0;
    for (const auto& test : registry()) {
        if (!matches_filter(test.name, filter)) {
            continue;
        }
        ++executed;
        try {
            test.fn();
            std::printf("[  ok  ] %s\n", test.name.c_str());
        } catch (const Failure& failure) {
            ++g_failures;
            std::printf("[ FAIL ] %s\n         %s (%s:%d)\n", test.name.c_str(),
                        failure.what(), failure.file(), failure.line());
        } catch (const std::exception& error) {
            ++g_failures;
            std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", test.name.c_str(),
                        error.what());
        }
    }

    std::printf("\n%d case(s), %d failure(s)\n", executed, g_failures);
    return g_failures == 0 ? 0 : 1;
}

}  // namespace lvtest

int main(int argc, char** argv) {
    return lvtest::run_all(argc, argv);
}
