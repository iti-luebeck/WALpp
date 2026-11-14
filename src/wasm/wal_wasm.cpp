// WebAssembly bridge for the documentation playground.
//
// Exposes a single WAL++ session to JavaScript through embind. The page keeps one
// Session per playground instance, writes traces into the Emscripten in-memory
// filesystem and evaluates WAL expressions against them. Everything an operator
// prints to stdout is captured per call, so `(print ...)` shows up in the console
// pane instead of the browser log.
//
// Built by build-wasm.sh; see docs/playground.md for how the page uses it.

#include <emscripten/bind.h>

#include "wal/core.h"
#include "wal/value_printer.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

constexpr const char* WORK_DIR = "/work";

/// Minimal JSON string escaping: the bridge hands results back as a JSON object
/// so the page can tell output, value and error apart without parsing prose.
std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

/// Redirects std::cout for as long as it is alive.
class CoutCapture {
public:
    CoutCapture() : old_(std::cout.rdbuf(buffer_.rdbuf())) {}
    ~CoutCapture() { restore(); }

    void restore() {
        if (old_) {
            std::cout.rdbuf(old_);
            old_ = nullptr;
        }
    }

    std::string str() const { return buffer_.str(); }

private:
    std::ostringstream buffer_;
    std::streambuf* old_;
};

class Session {
public:
    Session() { reset(); }

    /// Drops all traces and definitions and starts over.
    void reset() {
        wal_ = std::make_unique<wal::Wal>();
        ::mkdir(WORK_DIR, 0777);
    }

    /// Writes `content` into the in-memory filesystem and loads it as a trace.
    /// `name` must carry the extension, as the format is detected from it.
    /// Returns an empty string on success, otherwise the error message.
    std::string load_trace(const std::string& name,
                           const std::string& content,
                           const std::string& tid) {
        ::mkdir(WORK_DIR, 0777);
        const std::string path = std::string(WORK_DIR) + "/" + name;
        {
            std::ofstream f(path, std::ios::binary);
            if (!f) return "cannot write " + path + " into the in-memory filesystem";
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!f) return "failed while writing " + path;
        }
        try {
            wal_->load(path, tid.empty() ? "DEFAULT" : tid);
            return "";
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    /// Evaluates one WAL expression.
    /// Returns a JSON object {"out": printed, "val": result, "err": message}.
    /// `err` is empty when evaluation succeeded.
    std::string eval(const std::string& code) {
        std::string printed, value, error;
        {
            CoutCapture capture;
            try {
                wal::Value result = wal_->eval_str(code);
                std::ostringstream vs;
                wal::print_value(result, vs, /*quote_strings=*/true);
                capture.restore();
                value = vs.str();
            } catch (const std::exception& e) {
                capture.restore();
                error = e.what();
            }
            printed = capture.str();
        }
        return std::string("{\"out\":\"") + json_escape(printed)
             + "\",\"val\":\"" + json_escape(value)
             + "\",\"err\":\"" + json_escape(error) + "\"}";
    }

private:
    std::unique_ptr<wal::Wal> wal_;
};

} // namespace

EMSCRIPTEN_BINDINGS(walpp_playground) {
    emscripten::class_<Session>("Session")
        .constructor<>()
        .function("reset", &Session::reset)
        .function("loadTrace", &Session::load_trace)
        .function("eval", &Session::eval);
}
