#include "wal/core.h"
#include "wal/evaluator.h"
#include <iostream>

int main(int argc, char** argv) {
    wal::Wal w;
    if (argc > 1) w.load(argv[1], "t");

    // Register an operator written in C++. It receives its arguments UNEVALUATED,
    // so it decides what to evaluate and when.
    w.get_eval_context()->get_operator_registry().register_operator(
        "duty-cycle",
        [](wal::SEval& eval, const std::vector<wal::Value>& args) -> wal::Value {
            if (args.size() != 1) {
                throw wal::EvalError("duty-cycle expects one argument (a signal name)");
            }
            wal::Value name = eval.eval(args[0]);
            std::string signal = name.is_symbol() ? name.to_symbol()->get_name()
                                                  : name.to_string();
            auto traces = eval.get_traces();
            auto tid = traces->get_trace_ids().front();
            auto trace = traces->get_trace(tid);

            int high = 0, total = trace->get_max_index() + 1;
            int saved = trace->get_index();
            for (int i = 0; i < total; ++i) {
                trace->set_index(i);
                if (traces->signal_value(signal, 0).is_truthy()) ++high;
            }
            trace->set_index(saved);
            return wal::Value(static_cast<double>(high) / total);
        });

    std::cout << w.eval_str("(duty-cycle \"tb.clk\")").to_float() << "\n";
    return 0;
}
