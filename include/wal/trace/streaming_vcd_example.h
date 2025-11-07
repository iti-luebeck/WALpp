#pragma once

#include <wal/trace/streaming_chunk_reader.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>

namespace wal {

/**
 * @brief Example usage of StreamingChunkReader with VCD file
 * 
 * This demonstrates:
 * 1. Creating a streaming reader
 * 2. Iterating through chunks with C++20 coroutines
 * 3. Parsing VCD headers from chunks
 * 4. Building sparse index during streaming
 * 
 * **Key Principles:**
 * - Memory stays bounded (one chunk at a time)
 * - Lazy evaluation (data loaded on-demand)
 * - Perfect for terabyte-scale files
 */
class StreamingVCDExample {
public:
    /**
     * @brief Stream a VCD file and extract header information
     * 
     * Demonstrates:
     * - Opening a streaming reader
     * - Processing chunks incrementally
     * - Stopping after header is complete
     * - Reusing coroutine pattern for actual parsing
     */
    static void example_stream_vcd_header(const std::string& filename) {
        std::cout << "=== Streaming VCD Header Example ===" << std::endl;
        std::cout << "File: " << filename << std::endl;
        std::cout << std::endl;

        try {
            // Create reader with 100 MB chunks (suitable for VCD)
            auto reader = create_chunk_reader(filename, 100 * 1024 * 1024);

            size_t chunk_count = 0;
            size_t total_bytes = 0;

            // Iterate through chunks
            while (reader) {
                const auto& chunk = reader.get_chunk();
                chunk_count++;
                total_bytes += chunk.size();

                std::cout << "Chunk " << chunk_count << ": "
                          << std::setw(10) << chunk.size() << " bytes at offset "
                          << chunk.file_offset << std::endl;

                // In real implementation, you would:
                // 1. Parse chunk content
                // 2. Extract timestamps and signal changes
                // 3. Record in SparseIndex
                // 4. If header complete, break early

                // For now, just demonstrate iteration
                if (chunk.is_final_chunk) {
                    std::cout << "  (final chunk)" << std::endl;
                }

                // Advance to next chunk
                reader.next();
            }

            std::cout << std::endl;
            std::cout << "Summary:" << std::endl;
            std::cout << "  Total chunks: " << chunk_count << std::endl;
            std::cout << "  Total bytes: " << total_bytes << " ("
                      << (total_bytes / 1024.0 / 1024.0) << " MB)" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Demonstrate memory-efficient chunk processing
     * 
     * Shows how the same memory is reused for each chunk:
     * - Chunk 1: Read 100 MB → Process → Discard
     * - Chunk 2: Read 100 MB → Process → Discard
     * - Memory never exceeds 100 MB
     * 
     * This is impossible with load-all approach for 1TB files!
     */
    static void example_bounded_memory(const std::string& filename) {
        std::cout << "=== Bounded Memory Example ===" << std::endl;
        std::cout << "File: " << filename << std::endl;
        std::cout << std::endl;

        // Simulate processing with bounded memory
        const size_t CHUNK_SIZE = 50 * 1024 * 1024;  // 50 MB chunks
        const size_t MAX_MEMORY = 100 * 1024 * 1024; // 100 MB max

        try {
            auto reader = create_chunk_reader(filename, CHUNK_SIZE);

            size_t chunk_count = 0;
            size_t signals_found = 0;

            while (reader) {
                const auto& chunk = reader.get_chunk();
                chunk_count++;

                // In real code: parse chunk, extract signals
                // Memory usage = chunk.size() = 50 MB
                // Never exceeds MAX_MEMORY = 100 MB

                std::cout << "Processing chunk " << chunk_count << ": "
                          << chunk.size() / 1024 / 1024 << " MB" << std::endl;

                // Simulate finding some signals
                signals_found += chunk.size() / 1000000; // arbitrary

                reader.next();
            }

            std::cout << std::endl;
            std::cout << "Memory-efficient streaming complete!" << std::endl;
            std::cout << "  Chunks processed: " << chunk_count << std::endl;
            std::cout << "  Peak memory: " << CHUNK_SIZE / 1024 / 1024 << " MB" << std::endl;
            std::cout << "  Total memory available: " << MAX_MEMORY / 1024 / 1024 << " MB" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Performance comparison: Load-All vs Streaming
     * 
     * Demonstrates why streaming is necessary for terabyte files
     */
    static void example_memory_comparison() {
        std::cout << "=== Memory Comparison: Load-All vs Streaming ===" << std::endl;
        std::cout << std::endl;

        struct FileScenario {
            std::string name;
            int64_t size_bytes;
        };

        std::vector<FileScenario> scenarios = {
            {"Small trace (100 MB)", 100 * 1024 * 1024},
            {"Medium trace (1 GB)", 1024 * 1024 * 1024},
            {"Large trace (10 GB)", 10LL * 1024 * 1024 * 1024},
            {"Huge trace (1 TB)", 1000LL * 1024 * 1024 * 1024},
            {"Terabyte scale (10 TB)", 10000LL * 1024 * 1024 * 1024},
        };

        const size_t CHUNK_SIZE = 100 * 1024 * 1024; // 100 MB chunk

        std::cout << std::setw(30) << "Scenario"
                  << std::setw(20) << "Load-All Memory"
                  << std::setw(20) << "Streaming Memory"
                  << std::setw(15) << "Improvement"
                  << std::endl;
        std::cout << std::string(85, '-') << std::endl;

        for (const auto& scenario : scenarios) {
            // Load-all needs ~5x the file size for internal structures
            int64_t load_all_memory = scenario.size_bytes * 5;

            // Streaming only needs one chunk
            int64_t streaming_memory = CHUNK_SIZE;

            double improvement = static_cast<double>(load_all_memory) / streaming_memory;

            auto format_bytes = [](int64_t bytes) -> std::string {
                if (bytes < 1024) return std::to_string(bytes) + " B";
                if (bytes < 1024 * 1024) {
                    return std::to_string(bytes / 1024) + " KB";
                }
                if (bytes < 1024 * 1024 * 1024) {
                    return std::to_string(bytes / 1024 / 1024) + " MB";
                }
                return std::to_string(bytes / 1024 / 1024 / 1024) + " GB";
            };

            std::cout << std::setw(30) << scenario.name
                      << std::setw(20) << format_bytes(load_all_memory)
                      << std::setw(20) << format_bytes(streaming_memory)
                      << std::setw(13) << "x" << std::setprecision(1) << std::fixed
                      << improvement << std::endl;
        }

        std::cout << std::endl;
        std::cout << "Note: Load-All multiplier (5x) accounts for:" << std::endl;
        std::cout << "  - Original file data" << std::endl;
        std::cout << "  - Internal structures (maps, vectors)" << std::endl;
        std::cout << "  - Index tables" << std::endl;
        std::cout << "  - Value caches" << std::endl;
    }
};

} // namespace wal
