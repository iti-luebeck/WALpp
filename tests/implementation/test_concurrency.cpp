/**
 * @file test_concurrency.cpp
 * @brief What WAL++ guarantees across threads, and the state that used to break it.
 *
 * The guarantee is per-instance: separate `Wal` objects may be used from separate
 * threads at the same time. That is the shape a campaign has -- one trace per unit
 * of work -- and it scales about sevenfold on eight threads.
 *
 * Sharing ONE instance across threads is still unsupported, and these tests do not
 * claim otherwise. A Trace carries a cursor, so two threads stepping the same trace
 * would interleave their positions and produce plausible wrong answers rather than
 * a crash.
 *
 * What is tested here is the process-wide state that broke the per-instance
 * guarantee. Each case failed before the fix, with the numbers quoted.
 */
#include "wal/core.h"
#include "wal/evaluator.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what, const std::string& detail = "") {
    if (condition) {
        std::cout << "✅ PASS - " << what << "\n";
    } else {
        std::cout << "❌ FAIL - " << what;
        if (!detail.empty()) std::cout << ": " << detail;
        std::cout << "\n";
        ++failures;
    }
}

const char* TRACE = "tests/original/traces/counter.vcd";

bool trace_available() {
    std::ifstream f(TRACE);
    return f.good();
}

/**
 * Separate instances, used at the same time, must each give the right answer.
 */
void test_parallel_instances() {
    if (!trace_available()) {
        std::cout << "⏭️  SKIP - parallel instances (trace not found)\n";
        return;
    }

    const int threads = 8, per_thread = 10;
    std::atomic<int> wrong{0}, threw{0};

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&] {
            for (int i = 0; i < per_thread; ++i) {
                try {
                    wal::Wal w;
                    w.load(TRACE, "t");
                    if (w.eval_str("(count (rising tb.clk))").to_int() != 40) ++wrong;
                } catch (const std::exception&) {
                    ++threw;
                }
            }
        });
    }
    for (auto& t : pool) t.join();

    check(wrong.load() == 0 && threw.load() == 0,
          "separate Wal instances run concurrently",
          std::to_string(wrong.load()) + " wrong, " + std::to_string(threw.load()) + " threw");
}

/**
 * gensym exists to hand back a name nobody else holds. Its counter was a plain
 * function-local static, so `counter++` from two threads handed the same name to
 * both: 855 collisions in 4000 when this was written. Duplicate gensyms break macro
 * hygiene silently, as variable capture rather than as an error.
 */
void test_gensym_unique_across_threads() {
    const int threads = 8, each = 400;
    std::mutex m;
    std::vector<std::string> all;

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&] {
            wal::Wal w;
            std::vector<std::string> mine;
            mine.reserve(each);
            for (int i = 0; i < each; ++i) {
                mine.push_back(w.eval_str("(gensym)").to_symbol()->get_name());
            }
            std::lock_guard<std::mutex> g(m);
            all.insert(all.end(), mine.begin(), mine.end());
        });
    }
    for (auto& t : pool) t.join();

    std::set<std::string> unique(all.begin(), all.end());
    check(unique.size() == all.size(),
          "gensym is unique across threads",
          std::to_string(all.size() - unique.size()) + " collisions in " +
              std::to_string(all.size()));
}

/**
 * `require` tracked loaded modules in a process-wide set, which was wrong even on
 * one thread: a second instance skipped a module it had never loaded and so never
 * received its definitions. Module state belongs to the interpreter that loaded it.
 */
void test_require_is_per_instance() {
    const std::string path = "/tmp/walpp_concurrency_module.wal";
    {
        std::ofstream f(path);
        f << "(define from-module 7)\n";
    }

    bool first_ok = false, second_ok = false;
    try {
        wal::Wal a;
        a.eval_str("(require \"" + path + "\")");
        first_ok = a.eval_str("from-module").to_int() == 7;
    } catch (const std::exception&) {}

    try {
        wal::Wal b;
        b.eval_str("(require \"" + path + "\")");
        second_ok = b.eval_str("from-module").to_int() == 7;
    } catch (const std::exception&) {}

    check(first_ok && second_ok,
          "require loads a module into every instance",
          std::string("first=") + (first_ok ? "ok" : "FAILED") +
              " second=" + (second_ok ? "ok" : "FAILED"));

    // And concurrently.
    std::atomic<int> bad{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < 8; ++t) {
        pool.emplace_back([&] {
            for (int i = 0; i < 20; ++i) {
                try {
                    wal::Wal w;
                    w.eval_str("(require \"" + path + "\")");
                    if (w.eval_str("from-module").to_int() != 7) ++bad;
                } catch (const std::exception&) {
                    ++bad;
                }
            }
        });
    }
    for (auto& t : pool) t.join();

    check(bad.load() == 0, "require is safe across threads",
          std::to_string(bad.load()) + " instances failed");

    std::remove(path.c_str());
}

/**
 * The mode flags are read on every arithmetic operation and may be set from another
 * thread, so they are atomic. A relaxed load costs nothing measurable; what it buys
 * is that setting a mode is not a data race.
 */
void test_mode_flags() {
    wal::set_bigint_mode(false);
    wal::set_compat_types_mode(false);

    std::atomic<int> bad{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < 4; ++t) {
        pool.emplace_back([&] {
            for (int i = 0; i < 200; ++i) {
                wal::Wal w;
                // Reads the flags on every + and every =.
                if (w.eval_str("(+ 1 2)").to_int() != 3) ++bad;
                if (w.eval_str("(= 1 1)").to_bool() != true) ++bad;
            }
        });
    }
    for (auto& t : pool) t.join();

    check(bad.load() == 0, "mode flags are read safely while evaluating",
          std::to_string(bad.load()) + " wrong results");
}

} // namespace

int main() {
    std::cout << "=== Concurrency: per-instance guarantees ===\n\n";

    test_parallel_instances();
    test_gensym_unique_across_threads();
    test_require_is_per_instance();
    test_mode_flags();

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "All concurrency tests passed.\n";
        return 0;
    }
    std::cout << failures << " concurrency test(s) failed.\n";
    return 1;
}
