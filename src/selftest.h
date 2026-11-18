// Morpheus - --selftest runner.
// Every #[test] from crates/time and crates/project becomes a C++ test here.
// Morpheus.exe --selftest  runs all tests, prints PASS/FAIL per test, exits 0 on all pass.
// after EffectCraft (crates/time/src/lib.rs tests, crates/project/src/tests.rs)
#pragma once
#include <cstdio>
#include <string>
#include <functional>
#include <vector>

namespace SelfTest {

struct Test {
    std::string name;
    std::function<bool()> fn;  // returns true = pass
};

// Register a test (call before run()).
void add(const std::string& name, std::function<bool()> fn);

// Run all registered tests; print each result; return number of failures.
int run();

// Register all Morpheus tests (time + project).
// Call this before run().
void registerAll();

} // namespace SelfTest
