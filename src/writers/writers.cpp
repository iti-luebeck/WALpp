// writers.cpp - Writer factory and utility functions
// Phase 3 Task 1 - Trace File Writing API

#include "wal/writers/trace_writer.h"
#include "wal/writers/vcd_writer.h"
#include "wal/writers/fst_writer.h"
#include "wal/writers/csv_writer.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace wal::writers {

std::string detect_format(const std::string& filename) {
    if (filename.empty()) {
        return "unknown";
    }

    fs::path path(filename);
    std::string ext = path.extension().string();

    // Convert extension to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".vcd") {
        return "vcd";
    } else if (ext == ".fst") {
        return "fst";
    } else if (ext == ".csv") {
        return "csv";
    }

    return "unknown";
}

std::unique_ptr<TraceWriter> create_writer(
    const std::string& filename,
    const std::string& format) {
    if (filename.empty()) {
        throw std::runtime_error("Filename cannot be empty");
    }

    std::string actual_format = format;

    // Auto-detect format from extension if requested
    if (format == "auto" || format.empty()) {
        actual_format = detect_format(filename);
        if (actual_format == "unknown") {
            throw std::runtime_error(
                "Cannot auto-detect format from filename: " + filename
            );
        }
    }

    // Convert format to lowercase
    std::transform(actual_format.begin(), actual_format.end(),
                  actual_format.begin(), ::tolower);

    if (actual_format == "vcd") {
        return std::make_unique<VCDWriter>(filename);
    } else if (actual_format == "fst") {
        return std::make_unique<FSTWriter>(filename);
    } else if (actual_format == "csv") {
        return std::make_unique<CSVWriter>(filename);
    } else {
        throw std::runtime_error("Unsupported format: " + actual_format);
    }
}

}  // namespace wal::writers
