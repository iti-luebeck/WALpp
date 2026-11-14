#pragma once

#include "trace.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace wal {

/**
 * @brief A trace with no file behind it, created by `(new-trace id max-index)`.
 *
 * Upstream calls this a virtual trace. It has a timeline and nothing else: every
 * signal on it is a virtual signal, defined by `defsig` as an expression rather
 * than read from a dump. It is how a WAL program builds a derived trace, for
 * example a decoded view of a bus, and then writes it out with `dump-trace`.
 *
 * It holds no samples, so `contains_signal` and `get_all_signals` answer purely
 * from the virtual-signal map that Trace already carries.
 */
class VirtualTrace : public Trace {
public:
    VirtualTrace(const std::string& tid, int max, std::shared_ptr<TraceContainer> container)
        : Trace(tid, "", container) {
        max_index = max;
        current_index = 0;
    }

    /**
     * A virtual trace stores no samples. Reading a signal here means reading the
     * expression `defsig` left behind; evaluating it is the caller's job, because
     * only the evaluator knows the environment it was defined in.
     */
    Value signal_value(const std::string& name, int offset = 0,
                       const std::string& scope = "") override {
        (void)offset;
        (void)scope;
        auto it = virtual_signals.find(name);
        if (it == virtual_signals.end()) {
            throw std::runtime_error("Signal not found in virtual trace: " + name);
        }
        return it->second;
    }

    int signal_width(const std::string& name) override {
        // A virtual signal's width is whatever its expression yields, which is not
        // known without evaluating it against a trace. Upstream reports 0 too.
        (void)name;
        return 0;
    }

    bool contains_signal(const std::string& name) const override {
        return virtual_signals.count(name) > 0;
    }

    std::vector<std::string> get_all_signals() const override {
        std::vector<std::string> names;
        names.reserve(virtual_signals.size());
        for (const auto& entry : virtual_signals) {
            names.push_back(entry.first);
        }
        return names;
    }
};

} // namespace wal
