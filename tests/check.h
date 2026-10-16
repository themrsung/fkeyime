// Minimal test harness: no third-party test framework until the project needs one.
#pragma once

#include <cstdio>
#include <functional>
#include <vector>

namespace fkeyide::test {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Register {
    Register(const char* name, std::function<void()> body) {
        Registry().push_back({name, std::move(body)});
    }
};

inline int RunAll() {
    for (const auto& c : Registry()) {
        const int before = Failures();
        c.body();
        std::printf("%s %s\n", Failures() == before ? "PASS" : "FAIL", c.name);
    }
    std::printf("%zu cases, %d failed checks\n", Registry().size(), Failures());
    return Failures() == 0 ? 0 : 1;
}

}  // namespace fkeyide::test

#define FKEYIDE_CAT2(a, b) a##b
#define FKEYIDE_CAT(a, b) FKEYIDE_CAT2(a, b)
#define TEST_CASE(name)                                                          \
    static void FKEYIDE_CAT(test_, __LINE__)();                                  \
    static ::fkeyide::test::Register FKEYIDE_CAT(reg_, __LINE__)(name,           \
                                                                 FKEYIDE_CAT(test_, __LINE__)); \
    static void FKEYIDE_CAT(test_, __LINE__)()

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr); \
            ++::fkeyide::test::Failures();                                       \
        }                                                                        \
    } while (0)
