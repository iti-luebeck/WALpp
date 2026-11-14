#include "wal/trace/trace_container.h"
#include "wal/trace/virtual_trace.h"
#include "wal/trace/vcd_trace.h"
#include "wal/trace/csv_trace.h"
#include "wal/trace/fst_trace.h"
#include "wal/trace/streaming_vcd_trace.h"
#include "wal/trace/streaming_csv_trace.h"
#include "wal/trace/streaming_fst_trace.h"
#include "wal/trace/streaming_config.h"
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace wal {

// Load waveform file - auto-detect format
void TraceContainer::load(const std::string& file, const std::string& tid, 
                         const std::vector<std::string>* keep_signals) {
    try {
        // Check file exists and get size for streaming decision
        if (!std::filesystem::exists(file)) {
            throw std::runtime_error("File not found: " + file);
        }
        size_t file_size = std::filesystem::file_size(file);
        
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
        
        // Check if we should use streaming
        bool use_streaming = streaming_config.should_stream(file_size);
        
        // Create appropriate trace object based on file format and streaming preference
        std::shared_ptr<Trace> trace;
        
        if (extension == "vcd") {
            if (use_streaming) {
                load_streaming_vcd(file, trace_id, keep_signals);
            } else {
                load_vcd(file, trace_id, keep_signals);
            }
        }
        else if (extension == "csv") {
            if (use_streaming) {
                load_streaming_csv(file, trace_id, keep_signals);
            } else {
                load_csv(file, trace_id, keep_signals);
            }
        }
        else if (extension == "fst") {
            if (use_streaming) {
                load_streaming_fst(file, trace_id, keep_signals);
            } else {
                load_fst(file, trace_id, keep_signals);
            }
        }
        else {
            throw std::runtime_error("Unknown file format: " + extension + " (supported: vcd, csv, fst)");
        }
        
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
    
    // Trace::step already reports whether it could advance; this layer used to call
    // it and throw the bool away, returning "" unconditionally. That made `step`
    // always look successful, so `(while (step 1) ...)`, the standard WAL iteration
    // idiom, could never terminate. The tid of each trace that could not advance is
    // returned instead, which is also how upstream reports it.
    std::string ended;
    auto note = [&ended](const std::string& trace_id) {
        if (!ended.empty()) ended += " ";
        ended += trace_id;
    };

    if (!tid.empty()) {
        auto it = traces.find(tid);
        if (it != traces.end() && it->second) {
            if (!it->second->step(steps)) note(it->first);
        }
    } else {
        for (auto& [trace_id, trace] : traces) {
            if (trace) {
                if (!trace->step(steps)) note(trace_id);
            }
        }
    }
    return ended;
}

std::string TraceContainer::set_index(int idx, const std::string& tid) {
    if (traces.empty()) {
        return "";  // No traces loaded, silently succeed
    }

    if (!tid.empty()) {
        auto it = traces.find(tid);
        if (it != traces.end() && it->second) {
            it->second->set_index(idx);
        }
    } else {
        for (auto& [_, trace] : traces) {
            if (trace) {
                trace->set_index(idx);
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

void TraceContainer::new_virtual_trace(const std::string& tid, int max_index) {
    if (tid.empty()) {
        throw std::runtime_error("new-trace: trace id must not be empty");
    }
    if (traces.count(tid)) {
        throw std::runtime_error("new-trace: a trace with id " + tid + " is already loaded");
    }
    traces[tid] = std::make_shared<VirtualTrace>(tid, max_index,
                                                 std::make_shared<TraceContainer>(*this));
    num_traces++;
}

void TraceContainer::clear() {
    traces.clear();
    num_traces = 0;
}

// ============================================================================
// Standard (Full-Load) Format Loaders
// ============================================================================

void TraceContainer::load_vcd(const std::string& file, const std::string& tid,
                              const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<VCDTrace>(tid, file, std::make_shared<TraceContainer>(*this));
    auto vcd_trace = std::static_pointer_cast<VCDTrace>(trace);
    vcd_trace->load();
    traces[tid] = trace;
}

void TraceContainer::load_csv(const std::string& file, const std::string& tid,
                              const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<CSVTrace>(tid, file, std::make_shared<TraceContainer>(*this));
    auto csv_trace = std::static_pointer_cast<CSVTrace>(trace);
    csv_trace->load();
    traces[tid] = trace;
}

void TraceContainer::load_fst(const std::string& file, const std::string& tid,
                              const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<FSTTrace>(tid, file, std::make_shared<TraceContainer>(*this));
    auto fst_trace = std::static_pointer_cast<FSTTrace>(trace);
    fst_trace->load();
    traces[tid] = trace;
}

// ============================================================================
// Streaming Format Loaders (Complete Implementation)
// ============================================================================

void TraceContainer::load_streaming_vcd(const std::string& file, const std::string& tid,
                                        const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<StreamingVCDTrace>(
        tid, file,
        std::make_shared<TraceContainer>(*this),
        streaming_config.chunk_size_bytes,
        streaming_config.cache_capacity_per_signal
    );
    
    // Set up cache warming if configured
    if (!streaming_config.warm_cache_signals.empty()) {
        trace->warm_cache(streaming_config.warm_cache_signals);
    }
    
    trace->load();
    traces[tid] = trace;
    
    // Report cache stats if enabled
    if (streaming_config.report_cache_stats) {
        auto stats = get_cache_statistics(tid);
        if (!stats.empty()) {
            std::cerr << "[StreamingVCD] " << tid << ": " << stats << std::endl;
        }
    }
}

void TraceContainer::load_streaming_csv(const std::string& file, const std::string& tid,
                                        const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<StreamingCSVTrace>(
        tid, file,
        std::make_shared<TraceContainer>(*this),
        streaming_config.chunk_size_bytes,
        streaming_config.cache_capacity_per_signal
    );
    
    // Set up cache warming if configured
    if (!streaming_config.warm_cache_signals.empty()) {
        trace->warm_cache(streaming_config.warm_cache_signals);
    }
    
    trace->load();
    traces[tid] = trace;
    
    // Report cache stats if enabled
    if (streaming_config.report_cache_stats) {
        auto stats = get_cache_statistics(tid);
        if (!stats.empty()) {
            std::cerr << "[StreamingCSV] " << tid << ": " << stats << std::endl;
        }
    }
}

void TraceContainer::load_streaming_fst(const std::string& file, const std::string& tid,
                                        const std::vector<std::string>* keep_signals) {
    auto trace = std::make_shared<StreamingFSTTrace>(
        tid, file,
        std::make_shared<TraceContainer>(*this),
        streaming_config.chunk_size_bytes,
        streaming_config.cache_capacity_per_signal
    );
    
    // Set up cache warming if configured
    if (!streaming_config.warm_cache_signals.empty()) {
        trace->warm_cache(streaming_config.warm_cache_signals);
    }
    
    trace->load();
    traces[tid] = trace;
    
    // Report cache stats if enabled
    if (streaming_config.report_cache_stats) {
        auto stats = get_cache_statistics(tid);
        if (!stats.empty()) {
            std::cerr << "[StreamingFST] " << tid << ": " << stats << std::endl;
        }
    }
}

std::string TraceContainer::get_cache_statistics(const std::string& tid) const {
    auto trace_it = traces.find(tid);
    if (trace_it == traces.end()) {
        return "";
    }
    
    // Try to cast to streaming trace types and get statistics
    // Note: These are available only if streaming implementation is compiled
    #ifdef STREAMING_VCD_TRACE_ENABLED
    if (auto streaming_vcd = std::dynamic_pointer_cast<StreamingVCDTrace>(trace_it->second)) {
        return streaming_vcd->get_cache_statistics();
    }
    #endif
    
    #ifdef STREAMING_CSV_TRACE_ENABLED
    if (auto streaming_csv = std::dynamic_pointer_cast<StreamingCSVTrace>(trace_it->second)) {
        return streaming_csv->get_cache_statistics();
    }
    #endif
    
    #ifdef STREAMING_FST_TRACE_ENABLED
    if (auto streaming_fst = std::dynamic_pointer_cast<StreamingFSTTrace>(trace_it->second)) {
        return streaming_fst->get_cache_statistics();
    }
    #endif
    
    return "";  // Not a streaming trace or streaming not compiled
}

} // namespace wal


