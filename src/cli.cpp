#include "wal/cli.h"
#include "wal/agent_tools.h"
#include "wal/bigint.h"
#include "wal/core.h"
#include "wal/repl.h"
#include "wal/value_printer.h"
#include "wal/trace/streaming_config.h"
#include "wal/fast/vcd_parser.h"
#include "wal/fast/hamming_ops.h"
#include "wal/fast/streaming_ops.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <memory>

namespace wal {

CLI::CLI(int argc, const char* argv[])
    : program_name(argv[0]) {
    // Convert to vector for easier handling
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    wal_instance = std::make_shared<Wal>();
    repl_instance = std::make_shared<REPL>(wal_instance);
}

namespace {

/**
 * @brief Rewrite a verb invocation into the equivalent flags.
 *
 * `wal extract weight trace.vcd --scope top -o out.csv` becomes
 * `wal --fast-hw-scope top trace.vcd -o out.csv`.
 *
 * Desugaring rather than a second code path is the point. The flag forms are what
 * the published paper's artifact runs and what Table II was measured through, so
 * there must be exactly ONE implementation underneath; a verb that reached the
 * kernels by its own route could drift from the flag that the paper documents, and
 * nothing would say so.
 *
 * A first argument is a verb only when it matches one AND no file of that name
 * exists, so `wal lint` cannot be hijacked by a file called `lint` in the working
 * directory, and `wal script.wal` still runs a script.
 */
bool rewrite_verbs(std::vector<std::string>& args, bool& failed) {
    failed = false;
    if (args.empty()) return false;
    const std::string verb = args[0];
    static const std::set<std::string> verbs =
        {"inspect", "lint", "eval", "extract", "run", "repl"};
    if (!verbs.count(verb)) return false;
    if (std::filesystem::exists(verb)) return false;

    std::vector<std::string> rest(args.begin() + 1, args.end());
    std::vector<std::string> out;

    auto take_option = [&rest](const std::string& name, std::string& into) {
        for (size_t i = 0; i + 1 < rest.size(); ++i) {
            if (rest[i] == name) {
                into = rest[i + 1];
                rest.erase(rest.begin() + i, rest.begin() + i + 2);
                return true;
            }
        }
        return false;
    };

    if (verb == "repl") {
        // The REPL is what you get with no arguments, so the verb simply drops out
        // and any modifiers (a trace to load) pass through.
        out = rest;
    } else if (verb == "run") {
        out.push_back("-f");
        std::string script;
        for (size_t i = 0; i < rest.size(); ++i) {
            if (!rest[i].empty() && rest[i][0] != '-') {
                script = rest[i];
                rest.erase(rest.begin() + i);
                break;
            }
        }
        out.push_back(script);
        for (const auto& r : rest) out.push_back(r);
    } else if (verb == "inspect" || verb == "lint") {
        // `inspect` answers "what is in this trace", in whichever shape is asked
        // for. `extract` is left to mean only "produce a data series", so the two
        // verbs do not both answer the same question.
        std::string format = "json";
        if (verb == "inspect") {
            take_option("--format", format);
            for (size_t i = 0; i < rest.size(); ++i) {
                if (rest[i].rfind("--format=", 0) == 0) {
                    format = rest[i].substr(9);
                    rest.erase(rest.begin() + i);
                    break;
                }
            }
        }

        std::string subject;
        for (size_t i = 0; i < rest.size(); ++i) {
            if (!rest[i].empty() && rest[i][0] != '-') {
                subject = rest[i];
                rest.erase(rest.begin() + i);
                break;
            }
        }

        if (verb == "inspect" && (format == "names" || format == "list")) {
            out.push_back("--fast-list");
            out.push_back(subject);
            // --fast-list writes to a file; default it to stdout so `inspect` is
            // readable without one, as its JSON form is.
            bool has_output = false;
            for (const auto& r : rest) {
                if (r == "-o") has_output = true;
            }
            for (const auto& r : rest) out.push_back(r);
            if (!has_output) {
                out.push_back("-o");
                out.push_back("/dev/stdout");
            }
            args = out;
            return true;
        }
        if (verb == "inspect" && format != "json") {
            std::cerr << "Error: unknown inspect format '" << format
                      << "' (json or names)\n";
            failed = true;
            return false;
        }

        out.push_back(verb == "inspect" ? "--inspect" : "--lint");
        out.push_back(subject);
        for (const auto& r : rest) out.push_back(r);
    } else if (verb == "eval") {
        // every bare word is an expression; flags pass through
        for (size_t i = 0; i < rest.size(); ++i) {
            if (!rest[i].empty() && rest[i][0] != '-') {
                out.push_back("-e");
                out.push_back(rest[i]);
            } else {
                out.push_back(rest[i]);
                // -l and -o take a value
                if ((rest[i] == "-l" || rest[i] == "-o") && i + 1 < rest.size()) {
                    out.push_back(rest[++i]);
                }
            }
        }
    } else {  // extract
        if (rest.empty()) {
            std::cerr << "Error: extract needs a kind "
                         "(weight, distance or values)\n";
            failed = true;
            return false;
        }
        const std::string kind = rest.front();
        rest.erase(rest.begin());

        std::string scope, signals;
        take_option("--scope", scope);
        take_option("--signals", signals);

        std::string trace;
        for (size_t i = 0; i < rest.size(); ++i) {
            if (!rest[i].empty() && rest[i][0] != '-') {
                trace = rest[i];
                rest.erase(rest.begin() + i);
                break;
            }
        }

        std::string flag;
        if (kind == "names" || kind == "list") {
            std::cerr << "Error: a signal listing comes from "
                         "`inspect TRACE --format=names`;\n"
                         "       extract produces data series "
                         "(weight, distance or values)\n";
            failed = true;
            return false;
        } else if (kind == "values") {
            flag = "--fast-values-signals";
        } else if (kind == "weight") {
            flag = !scope.empty()   ? "--fast-hw-scope"
                 : !signals.empty() ? "--fast-hw-signals" : "--fast-hw-all";
        } else if (kind == "distance") {
            flag = !scope.empty()   ? "--fast-hd-scope"
                 : !signals.empty() ? "--fast-hd-signals" : "--fast-hd-all";
        } else {
            std::cerr << "Error: unknown extract kind '" << kind
                      << "' (weight, distance or values)\n";
            failed = true;
            return false;
        }

        out.push_back(flag);
        // The flag forms take the selector before the trace.
        if (flag.find("-scope") != std::string::npos) out.push_back(scope);
        if (flag.find("-signals") != std::string::npos) {
            if (signals.empty()) {
                std::cerr << "Error: extract " << kind << " needs --signals <file>\n";
                failed = true;
                return false;
            }
            out.push_back(signals);
        }
        out.push_back(trace);
        for (const auto& r : rest) out.push_back(r);
    }

    args = out;
    return true;
}

} // namespace

bool CLI::parse_arguments(int argc, const char* argv[]) {
    // Verbs are sugar over the flags below; see rewrite_verbs(). A verb used wrongly
    // stops here rather than falling through to flag parsing, where the verb word
    // would be taken as a filename and the real error buried under a second one.
    rewrite_verbs(args, verb_usage_error);

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg == "-h" || arg == "--help") {
            help_flag = true;
        } else if (arg == "-v" || arg == "--version") {
            version_flag = true;
        } else if (arg == "--fast-hd-all") {
            fast_hd_all = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hd-scope") {
            fast_hd_scope = true;
            if (i + 1 < args.size()) fast_scope = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--inspect") {
            inspect_mode = true;
            if (i + 1 < args.size()) inspect_file = args[++i];
        } else if (arg == "--lint") {
            lint_mode = true;
            if (i + 1 < args.size()) lint_file = args[++i];
        } else if (arg == "--json") {
            json_output = true;
        } else if (arg == "--compact-json") {
            json_output = true;
            pretty_json = false;
        } else if (arg == "--compat-types") {
            // Upstream's permissive coercion, for running upstream scripts
            // unchanged. Off by default: see compat_types_mode().
            set_compat_types_mode(true);
        } else if (arg == "--bigint") {
            // Opt-in arbitrary-precision integers. Off by default: 64-bit is what
            // waveform values and indices need, and the bigint path is slower.
            set_bigint_mode(true);
        } else if (arg == "--fast-hw-all") {
            fast_hw_all = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hw-scope") {
            fast_hw_scope = true;
            if (i + 1 < args.size()) fast_scope = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hd-signals") {
            fast_hd_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hw-signals") {
            fast_hw_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-values-signals") {
            fast_values_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-list") {
            fast_list = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "-e" || arg == "--eval") {
            eval_mode = true;
            if (i + 1 < args.size()) {
                // Accumulate: several -e run in order and share one interpreter,
                // so (define ...) in an earlier -e is visible to a later one.
                eval_exprs.push_back(args[++i]);
            }
        } else if (arg == "-f" || arg == "--file") {
            file_mode = true;
            if (i + 1 < args.size()) {
                filename = args[++i];
            }
        } else if (arg == "-l" || arg == "--load") {
            trace_mode = true;
            if (i + 1 < args.size()) {
                trace_file = args[++i];
            }
        } else if (arg == "--pipe") {
            pipe_mode = true;
            if (i + 1 < args.size()) {
                pipe_file = args[++i];
            }
        } else if (arg == "--stdin") {
            stdin_mode = true;
        } else if (arg == "--streaming") {
            streaming_enabled = true;
            streaming_explicit = true;
        } else if (arg == "--no-streaming") {
            streaming_enabled = false;
            streaming_explicit = true;
        } else if (arg == "--chunk-size") {
            if (i + 1 < args.size()) {
                try {
                    size_t size_mb = std::stoul(args[++i]);
                    chunk_size_bytes = size_mb * 1024 * 1024;
                } catch (...) {
                    std::cerr << "Invalid chunk size: " << args[i] << std::endl;
                }
            }
        } else if (arg == "--cache-capacity") {
            if (i + 1 < args.size()) {
                try {
                    cache_capacity = std::stoul(args[++i]);
                } catch (...) {
                    std::cerr << "Invalid cache capacity: " << args[i] << std::endl;
                }
            }
        } else if (arg == "--cache-stats") {
            // This flag will be handled during trace loading
            // Store it for later use in run()
        } else if (arg[0] != '-') {
            // Positional argument - assume it's a file
            file_mode = true;
            filename = arg;
        }
    }
    
    return true;
}

void CLI::print_usage() {
    std::cout <<
"Usage: " << program_name << R"( <verb> [options]        (recommended)
       )" << program_name << R"( [flags] [FILE]          (equivalent; what the paper documents)

VERBS
  inspect TRACE [--format=F]        what is in a trace
        F = json (default) | names    names gives "width name" + a timescale header
  lint SCRIPT.wal                   check a script without running it
  eval EXPR [EXPR...]               evaluate expressions
  run SCRIPT.wal                    run a script
  repl                              start the interactive REPL
  extract KIND TRACE -o OUT.csv     one pass over a trace, CSV out
        KIND = weight | distance | values
        [--scope SCOPE]             only signals under a hierarchy prefix
        [--signals FILE]            only the signals named in FILE

  Common to all verbs: -l TRACE, --json, --compact-json

EXAMPLES
  )" << program_name << R"( inspect trace.vcd --json
  )" << program_name << R"( inspect trace.vcd --format=names > signals.txt
  )" << program_name << R"( extract weight trace.vcd --scope top.core -o activity.csv
  )" << program_name << R"( eval '(count (rising tb.clk))' -l trace.vcd
  )" << program_name << R"( lint analysis.wal

GENERAL FLAGS
  -h, --help                        this message
  -v, --version                     version information
  -e, --eval EXPR                   evaluate an expression
  -f, --file FILE                   run a WAL script
  -l, --load FILE                   load a trace

OUTPUT
  --json                            results and findings as JSON, in the
                                    standard envelope (ok / data / error)
  --compact-json                    the same, on one line

LANGUAGE
  --compat-types                    upstream's permissive type coercion
                                    (default: types stay separate)
  --bigint                          arbitrary-precision integers
                                    (default: 64-bit, which is faster)

EXTRACTION FLAGS  (equivalent to `extract`; these are what the published
                   artifact scripts use, and they are not going away)
  --fast-hw-all FILE -o OUT             = extract weight FILE
  --fast-hw-scope SCOPE FILE -o OUT     = extract weight FILE --scope SCOPE
  --fast-hw-signals LIST FILE -o OUT    = extract weight FILE --signals LIST
  --fast-hd-all FILE -o OUT             = extract distance FILE
  --fast-hd-scope SCOPE FILE -o OUT     = extract distance FILE --scope SCOPE
  --fast-hd-signals LIST FILE -o OUT    = extract distance FILE --signals LIST
  --fast-values-signals LIST FILE -o OUT = extract values FILE --signals LIST
  --fast-list FILE -o OUT               = inspect FILE --format=names

  weight   = Hamming weight: bits set at each timestamp
  distance = Hamming distance: bits changed since the previous timestamp
  values   = the decoded value of each listed signal, when it changes
  (for a signal listing, see `inspect --format=names`)

AGENT MODES (flag spelling)
  --inspect FILE                    = inspect FILE
  --lint FILE.wal                   = lint FILE.wal

INPUT MODES
  --pipe FILE                       read trace data from a named pipe
  --stdin                           read trace data from standard input

STREAMING
  --streaming                       always stream the trace
  --no-streaming                    never stream, whatever the size
  (neither)                         stream only above --auto-threshold
  --chunk-size SIZE                 chunk size in MB (default: 100)
  --cache-capacity NUM              cache entries per signal (default: 1000)
)";
}

void CLI::print_help() {
    print_usage();
    std::cout << "For interactive help, start the REPL and type (help)\n";
}

void CLI::print_version() {
    std::cout << "WAL++ v" WALPP_VERSION "\n"
              << "Language: S-expressions with 130+ operators\n"
              << "Build: C++20 with pybind11 Python bindings\n"
              << "Features: Streaming trace support (VCD, CSV, FST)\n";
}

void CLI::apply_streaming_config() {
    // Get the global trace container from the WAL instance
    // Configure streaming options before any trace loading
    if (wal_instance) {
        auto container = wal_instance->get_traces();
        if (container) {
            // Apply streaming enable/disable if explicitly set
            if (streaming_explicit) {
                container->set_streaming_enabled(streaming_enabled);
            }
            
            // Create and apply configuration.
            //
            // Three states, which the flags did not previously distinguish:
            //   --streaming      always stream
            //   --no-streaming   never stream
            //   neither          stream only when the file is above the threshold
            //
            // `--no-streaming` used to leave the auto-threshold in place, so a file
            // over 100 MB streamed anyway -- exactly the file someone passes the
            // flag for. Clearing the threshold is what makes "never" mean never.
            StreamingConfig config;
            config.enabled = streaming_enabled;
            if (streaming_explicit && !streaming_enabled) {
                config.auto_threshold_bytes = 0;
            }
            config.chunk_size_bytes = chunk_size_bytes;
            config.cache_capacity_per_signal = cache_capacity;
            config.report_cache_stats = false;  // Stats will be printed manually if requested
            
            container->set_streaming_config(config);
        }
    }
}

int CLI::run_fast_mode() {
    using namespace std::chrono;
    
    // Helper lambda to format duration
    auto format_duration = [](int64_t ms) -> std::string {
        if (ms < 1000) {
            return std::to_string(ms) + "ms";
        } else if (ms < 60000) {
            double seconds = ms / 1000.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << seconds << "s";
            return oss.str();
        } else if (ms < 3600000) {
            int minutes = ms / 60000;
            double seconds = (ms % 60000) / 1000.0;
            std::ostringstream oss;
            oss << minutes << "m " << std::fixed << std::setprecision(1) << seconds << "s";
            return oss.str();
        } else {
            int hours = ms / 3600000;
            int minutes = (ms % 3600000) / 60000;
            int seconds = (ms % 60000) / 1000;
            std::ostringstream oss;
            oss << hours << "h " << minutes << "m " << seconds << "s";
            return oss.str();
        }
    };
    
    if (fast_vcd_file.empty() || fast_output_file.empty()) {
        std::cerr << "Error: VCD file and output file required for fast mode\n";
        print_usage();
        return 1;
    }
    
    try {
        // Detect file format by extension
        auto ends_with = [](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        const bool is_fst = ends_with(fast_vcd_file, ".fst");

        // Parse VCD or FST
        auto parse_start = high_resolution_clock::now();

        fast_vcd::VCDParser parser;

        if (fast_hd_scope || fast_hw_scope) {
            if (fast_scope.empty()) {
                std::cerr << "Error: Scope required for scoped operations\n";
                return 1;
            }
            if (is_fst) {
                std::cerr << "Parsing FST with scope filter: " << fast_scope << "...\n";
                parser.parse_fst(fast_vcd_file, fast_scope);
            } else {
                std::cerr << "Parsing VCD with scope filter: " << fast_scope << "...\n";
                parser.parse(fast_vcd_file, fast_scope);
            }
        } else if (fast_hd_signals || fast_hw_signals || fast_values_signals) {
            if (fast_signals_file.empty()) {
                std::cerr << "Error: Signal list file required for signal list operations\n";
                return 1;
            }
            if (is_fst) {
                std::cerr << "Parsing FST (will filter by signal list later)...\n";
                parser.parse_fst(fast_vcd_file);
            } else {
                std::cerr << "Parsing VCD (will filter by signal list later)...\n";
                parser.parse(fast_vcd_file);
            }
        } else {
            if (is_fst) {
                std::cerr << "Parsing FST...\n";
            } else {
                std::cerr << "Parsing VCD...\n";
            }
        }

        // Read the signal list before parsing: with streaming the kernel runs
        // DURING the parse, so it has to exist first.
        std::vector<std::string> signal_names;
        if (fast_hd_signals || fast_hw_signals || fast_values_signals) {
            std::ifstream sigfile(fast_signals_file);
            if (!sigfile) {
                std::cerr << "Error: Could not open signal list file: " << fast_signals_file << "\n";
                return 1;
            }
            std::string line;
            while (std::getline(sigfile, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (!line.empty() && line[0] != '#') {
                    signal_names.push_back(line);
                }
            }
            sigfile.close();
            std::cerr << "Loaded " << signal_names.size() << " signal names from list\n";
        }

        std::ofstream outfile;
        auto open_output = [&]() -> bool {
            outfile.open(fast_output_file);
            if (!outfile) {
                std::cerr << "Error: Could not open output file: " << fast_output_file << "\n";
                return false;
            }
            return true;
        };

        // Pick the kernel and install it as the parser's delta sink, so each
        // timestamp is consumed and dropped as it is read. Nothing accumulates: peak
        // memory is O(signals) rather than growing with the trace.
        //
        // FST is parsed through GTKWave's fstapi, which hands over its own callback
        // structure, so that path still materialises the deltas and then replays
        // them through the same kernel below. The results are identical either way;
        // only the memory differs.
        size_t rows_written = 0;
        bool have_kernel = false;

        std::unique_ptr<fast_vcd::StreamingHamming> hamming;
        std::unique_ptr<fast_vcd::StreamingValues> values;

        if (fast_values_signals) {
            if (!open_output()) return 1;
            std::cerr << "Extracting value changes (" << signal_names.size() << " signals)...\n";
            // The timescale is only known once the header has been read, which
            // happens inside parse() just before the first delta arrives, so the
            // header line is written lazily on the first row rather than up front.
            values = std::make_unique<fast_vcd::StreamingValues>(
                parser, signal_names,
                [&outfile, &parser, wrote_header = false](int64_t ts, uint32_t idx,
                                                          int64_t val) mutable {
                    if (!wrote_header) {
                        outfile << "# timescale " << parser.get_timescale() << "\n";
                        wrote_header = true;
                    }
                    outfile << ts << "," << idx << "," << val << "\n";
                });
            have_kernel = true;
        } else if (fast_hd_all || fast_hd_scope || fast_hd_signals ||
                   fast_hw_all || fast_hw_scope || fast_hw_signals) {
            bool distance = fast_hd_all || fast_hd_scope || fast_hd_signals;
            fast_vcd::TrackedSignals tracked = fast_vcd::TrackedSignals::all();
            if (fast_hd_scope || fast_hw_scope) {
                tracked = fast_vcd::TrackedSignals::in_scope(fast_scope);
            } else if (fast_hd_signals || fast_hw_signals) {
                tracked = fast_vcd::TrackedSignals::named(signal_names);
            }
            if (!open_output()) return 1;
            std::cerr << "Computing Hamming " << (distance ? "Distance" : "Weight") << " (";
            if (fast_hd_scope || fast_hw_scope) std::cerr << "scope: " << fast_scope;
            else if (fast_hd_signals || fast_hw_signals) std::cerr << signal_names.size() << " signals";
            else std::cerr << "all signals";
            std::cerr << ")...\n";
            hamming = std::make_unique<fast_vcd::StreamingHamming>(
                parser, tracked, distance,
                [&outfile](int64_t ts, int64_t value) {
                    outfile << ts << "," << value << "\n";
                });
            have_kernel = true;
        }

        // --fast-list needs only the header, so a sink that discards every delta
        // keeps it from building a structure it will not read.
        if (fast_list && !is_fst) {
            parser.set_delta_sink([](const fast_vcd::DeltaTimestamp&) {});
        }

        if (have_kernel && !is_fst) {
            parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& delta) {
                if (hamming) hamming->on_delta(delta);
                else if (values) values->on_delta(delta);
            });
        }

        if (is_fst) {
            parser.parse_fst(fast_vcd_file);
        } else {
            parser.parse(fast_vcd_file);
        }

        auto parse_end = high_resolution_clock::now();
        auto parse_ms = duration_cast<milliseconds>(parse_end - parse_start).count();

        std::cerr << "Parsed " << parser.num_signals() << " signals, "
                  << parser.num_timestamps() << " timestamps in "
                  << format_duration(parse_ms) << "\n";

        auto op_start = high_resolution_clock::now();

        // --fast-list needs only the header.
        if (fast_list) {
            std::ofstream out(fast_output_file);
            if (!out) {
                std::cerr << "Error: Could not open output file: " << fast_output_file << "\n";
                return 1;
            }
            out << "# timescale " << parser.get_timescale() << "\n";
            for (uint32_t id : parser.get_signal_ids()) {
                const auto& info = parser.get_signal_info(id);
                out << info.width << " " << info.name << "\n";
            }
            std::cerr << "Listed " << parser.num_signals() << " signals to " << fast_output_file << "\n";
            return 0;
        }

        if (!have_kernel) {
            std::cerr << "Error: no extraction mode selected\n";
            return 1;
        }

        // The values output carries a timescale header; it has to precede the rows,
        // so it is written before the parse when streaming. For FST the rows have
        // not been produced yet, so replay the stored deltas through the kernel now.
        if (is_fst) {
            for (const auto& delta : parser.get_deltas()) {
                if (hamming) hamming->on_delta(delta);
                else if (values) values->on_delta(delta);
            }
        }

        if (hamming) { hamming->finish(); rows_written = hamming->rows(); }
        if (values)  { values->finish();  rows_written = values->rows(); }
        outfile.close();

        auto op_end = high_resolution_clock::now();
        auto op_ms = duration_cast<milliseconds>(op_end - op_start).count();

        std::cerr << "Computation completed in " << format_duration(op_ms) << "\n";
        std::cerr << "Total time: " << format_duration(parse_ms + op_ms) << "\n";
        std::cerr << "Results written to: " << fast_output_file
                  << " (" << rows_written << " entries)\n";

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int CLI::run() {
    parse_arguments(args.size(), nullptr);

    if (verb_usage_error) {
        return 1;          // the message was already printed by rewrite_verbs()
    }
    
    // Handle flags
    if (help_flag) {
        print_help();
        return 0;
    }
    
    if (version_flag) {
        print_version();
        return 0;
    }
    
    // Machine-readable modes. These come before everything else because they are
    // self-contained: each answers one question and exits, so a caller never has to
    // separate the answer from REPL chatter or a trace-loading message.
    if (lint_mode) {
        if (lint_file.empty()) {
            std::cerr << "Error: --lint needs a script file\n";
            return 1;
        }
        std::ifstream in(lint_file);
        if (!in) {
            std::cerr << "Error: cannot open " << lint_file << "\n";
            return 1;
        }
        std::ostringstream text;
        text << in.rdbuf();
        auto findings = lint_script(text.str(), lint_file);
        if (json_output) {
            bool clean = true;
            for (const auto& f : findings) {
                if (f.severity == LintFinding::Severity::Error) clean = false;
            }
            if (clean) {
                std::cout << envelope("lint",
                                      lint_findings_json(findings, lint_file, pretty_json),
                                      lint_file, pretty_json) << "\n";
            } else {
                // A script that will not run is a failure of the command, so it
                // carries the failure envelope. The findings stay in `data`, since
                // they are the answer, not a side note.
                AgentError err{"walpp.lint.failed",
                               "the script has errors and will not run",
                               lint_file,
                               "see data.findings for the locations"};
                std::string body = error_envelope("lint", err, lint_file, pretty_json);
                auto cut = body.rfind("\"data\"");
                if (cut != std::string::npos) {
                    body = body.substr(0, cut) + "\"data\"" +
                           std::string(pretty_json ? ": " : ":") +
                           lint_findings_json(findings, lint_file, pretty_json) +
                           (pretty_json ? "\n}" : "}");
                }
                std::cout << body << "\n";
            }
        } else {
            std::cout << lint_findings_text(findings, lint_file);
        }
        // Non-zero when the script will not run, so this drops into a build or a
        // pre-commit hook without a wrapper.
        for (const auto& f : findings) {
            if (f.severity == LintFinding::Severity::Error) return 1;
        }
        return 0;
    }

    if (inspect_mode) {
        if (inspect_file.empty()) {
            std::cerr << "Error: --inspect needs a trace file\n";
            return 1;
        }
        try {
            std::cout << envelope("inspect", inspect_trace(inspect_file, pretty_json),
                                  inspect_file, pretty_json) << "\n";
            return 0;
        } catch (const std::exception& e) {
            // Same envelope as a success: a consumer never has to tell failure from
            // an empty result by reading prose.
            AgentError err{"walpp.trace.unreadable", e.what(), inspect_file,
                           "check the path and that the format is VCD, FST or CSV"};
            std::cout << error_envelope("inspect", err, inspect_file, pretty_json) << "\n";
            return 1;
        }
    }

    // Check for fast mode first (highest priority)
    if (fast_hd_all || fast_hd_scope || fast_hw_all || fast_hw_scope ||
        fast_hd_signals || fast_hw_signals || fast_values_signals || fast_list) {
        return run_fast_mode();
    }
    
    // Apply streaming configuration early
    apply_streaming_config();
    
    // Load trace file if specified
    if (trace_mode && !trace_file.empty()) {
        try {
            // `-l` used to force streaming off here unconditionally, with a note
            // that the hamming operators needed full trace access. That silently
            // ignored --streaming on the main entry point, and the reason no longer
            // holds: the extraction modes never build a Trace at all, and the
            // streaming reader now answers indexed reads. Honour what was asked.
            wal_instance->load(trace_file);
        } catch (const std::exception& e) {
            std::cerr << "Error loading trace file: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // Eval mode
    if (eval_mode && !eval_exprs.empty()) {
        std::vector<std::string> json_results;
        for (const auto& expr : eval_exprs) {
            try {
                Value result = wal_instance->eval_str(expr);

                // Print result. Unlike the REPL, -e echoes nil, so a caller can
                // tell "evaluated to nothing" from "produced no output". Strings
                // print bare so a shell can consume them. Lists and symbols are
                // printed by the shared renderer: they used to fall off the end
                // of this chain and vanish, which made `-e '(signal-names)'`
                // look like an empty trace.
                if (json_output) {
                    // Collected, not streamed: one command produces one envelope, so
                    // `ok` describes the whole run rather than whichever -e printed last.
                    json_results.push_back(value_to_json(result));
                } else if (result.is_nil()) {
                    std::cout << "nil" << std::endl;
                } else {
                    print_value(result, std::cout, /*quote_strings=*/false);
                    std::cout << std::endl;
                }
            } catch (const std::exception& e) {
                if (json_output) {
                    // The failure envelope carries the results that did succeed, so a
                    // caller can see how far the run got.
                    AgentError err{"walpp.eval.failed", e.what(), expr,
                                   "the expressions before this one are in data.results"};
                    std::string body = error_envelope("eval", err, trace_file, pretty_json);
                    auto cut = body.rfind("\"data\"");
                    if (cut != std::string::npos) {
                        std::ostringstream partial;
                        partial << "{\"results\":[";
                        for (size_t k = 0; k < json_results.size(); ++k) {
                            if (k) partial << ",";
                            partial << json_results[k];
                        }
                        partial << "]}";
                        body = body.substr(0, cut) + "\"data\"" +
                               std::string(pretty_json ? ": " : ":") + partial.str() +
                               (pretty_json ? "\n}" : "}");
                    }
                    std::cout << body << std::endl;
                } else {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
                return 1;
            }
        }
        if (json_output) {
            std::ostringstream payload;
            payload << "{" << (pretty_json ? "\n    " : "") << "\"results\""
                    << (pretty_json ? ": " : ":") << "[";
            for (size_t k = 0; k < json_results.size(); ++k) {
                if (k) payload << ",";
                payload << json_results[k];
            }
            payload << "]" << (pretty_json ? "\n  " : "") << "}";
            std::cout << envelope("eval", payload.str(), trace_file, pretty_json)
                      << std::endl;
        }
        return 0;
    }
    
    // File mode
    if (file_mode && !filename.empty()) {
        return repl_instance->load_file(filename) ? 0 : 1;
    }
    
    // Pipe mode
    if (pipe_mode && !pipe_file.empty()) {
        return repl_instance->load_pipe(pipe_file) ? 0 : 1;
    }
    
    // Stdin mode
    if (stdin_mode) {
        return repl_instance->read_from_stdin() ? 0 : 1;
    }
    
    // Default: REPL mode
    return repl_instance->run();
}

} // namespace wal
