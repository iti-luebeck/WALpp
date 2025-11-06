#include "wal/trace/trace_container.h"
#include "wal/trace/vcd_trace.h"
#include "wal/trace/csv_trace.h"
#include "wal/trace/fst_trace.h"
#include <stdexcept>
#include <filesystem>

namespace wal {

// Load waveform file - auto-detect format
void TraceContainer::load(const std::string& file, const std::string& tid, 
                         const std::vector<std::string>* keep_signals) {
    try {
        // Determine file format from extension
        std::string extension;
        size_t dot_pos = file.find_last_of('.');
        if (dot_pos != std::string::npos) {
            extension = file.substr(dot_pos + 1);
            // Convert to lowercase
            std::transform(extension.begin(), extension.end(), extension.begin(),
                         [](unsigned char c) { return std::tolower(c); });
        }
        
        // Generate trace ID if not provided
        std::string trace_id = tid;
        if (trace_id.empty()) {
            trace_id = "trace_" + std::to_string(num_traces + 1);
        }
        
        // Create appropriate trace object based on file format
        std::shared_ptr<Trace> trace;
        
        if (extension == "vcd") {
            trace = std::make_shared<VCDTrace>(trace_id, file, std::make_shared<TraceContainer>(*this));
            auto vcd_trace = std::static_pointer_cast<VCDTrace>(trace);
            vcd_trace->load();
        }
        else if (extension == "csv") {
            trace = std::make_shared<CSVTrace>(trace_id, file, std::make_shared<TraceContainer>(*this));
            auto csv_trace = std::static_pointer_cast<CSVTrace>(trace);
            csv_trace->load();
        }
        else if (extension == "fst") {
            trace = std::make_shared<FSTTrace>(trace_id, file, std::make_shared<TraceContainer>(*this));
            auto fst_trace = std::static_pointer_cast<FSTTrace>(trace);
            fst_trace->load();
        }
        else {
            throw std::runtime_error("Unknown file format: " + extension + " (supported: vcd, csv, fst)");
        }
        
        // Store loaded trace
        traces[trace_id] = trace;
        num_traces++;
        
    } catch (const std::exception& e) {
        throw WalError("Failed to load trace: " + std::string(e.what()));
    }
}

void TraceContainer::unload(const std::string& tid) {
    auto it = traces.find(tid);
    if (it != traces.end()) {
        traces.erase(it);
        num_traces--;
    }
}

Value TraceContainer::signal_value(const std::string& name, int offset, 
                                   const std::string& scope) {
    // Search all traces for the signal
    for (auto& [_, trace] : traces) {
        if (trace && trace->contains_signal(name)) {
            return trace->signal_value(name, offset, scope);
        }
    }
    throw WalError("Signal not found in any loaded trace: " + name);
}

int TraceContainer::signal_width(const std::string& name) {
    // Search all traces for the signal
    for (auto& [_, trace] : traces) {
        if (trace && trace->contains_signal(name)) {
            return trace->signal_width(name);
        }
    }
    throw WalError("Signal not found in any loaded trace: " + name);
}

bool TraceContainer::contains(const std::string& name) {
    // Search all traces for the signal
    for (auto& [_, trace] : traces) {
        if (trace && trace->contains_signal(name)) {
            return true;
        }
    }
    return false;
}

std::string TraceContainer::step(int steps, const std::string& tid) {
    if (traces.empty()) {
        return "";  // No traces loaded, silently succeed
    }
    
    if (!tid.empty()) {
        // Step specific trace
        auto it = traces.find(tid);
        if (it != traces.end() && it->second) {
            it->second->step(steps);
        }
    } else {
        // Step all traces
        for (auto& [_, trace] : traces) {
            if (trace) {
                trace->step(steps);
            }
        }
    }
    return "";
}

std::unordered_map<std::string, int> TraceContainer::indices() const {
    std::unordered_map<std::string, int> result;
    for (const auto& [tid, trace] : traces) {
        if (trace) {
            result[tid] = trace->get_index();
        }
    }
    return result;
}

std::vector<std::string> TraceContainer::get_trace_ids() const {
    std::vector<std::string> ids;
    for (const auto& [id, _] : traces) {
        ids.push_back(id);
    }
    return ids;
}

std::shared_ptr<Trace> TraceContainer::get_trace(const std::string& tid) const {
    auto it = traces.find(tid);
    if (it != traces.end()) {
        return it->second;
    }
    return nullptr;
}

void TraceContainer::clear() {
    traces.clear();
    num_traces = 0;
}

} // namespace wal


