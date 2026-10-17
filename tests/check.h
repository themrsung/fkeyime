// Minimal test harness: no third-party test framework until the project needs one.
#pragma once

#include <cstdio>
#include <functional>
#include <vector>

namespace fkeyime::test {

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

}  // namespace fkeyime::test

#define FKEYIME_CAT2(a, b) a##b
#define FKEYIME_CAT(a, b) FKEYIME_CAT2(a, b)
#define TEST_CASE(name)                                                          \
    static void FKEYIME_CAT(test_, __LINE__)();                                  \
    static ::fkeyime::test::Register FKEYIME_CAT(reg_, __LINE__)(name,           \
                                                                 FKEYIME_CAT(test_, __LINE__)); \
    static void FKEYIME_CAT(test_, __LINE__)()

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr); \
            ++::fkeyime::test::Failures();                                       \
        }                                                                        \
    } while (0)
