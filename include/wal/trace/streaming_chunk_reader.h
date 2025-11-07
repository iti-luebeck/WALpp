#pragma once

#include <coroutine>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <span>
#include <utility>

namespace wal {

/**
 * @brief Chunk of trace data read from file
 * 
 * Represents a fixed-size chunk of bytes from the trace file.
 * Used by StreamingChunkReader to yield data chunks on-demand.
 */
struct TraceChunk {
    /// Start byte offset in file
    int64_t file_offset;
    
    /// Raw bytes from file
    std::vector<char> data;
    
    /// Whether this is the final chunk
    bool is_final_chunk;
    
    /// Number of valid bytes in data
    size_t size() const { return data.size(); }
    
    /// Access raw data as span
    std::span<const char> as_span() const {
        return std::span<const char>(data.data(), data.size());
    }
};

// Forward declarations
class StreamingChunkReader;

/**
 * @brief Promise type for StreamingChunkReader coroutine
 * 
 * Implements the coroutine protocol for yielding TraceChunk objects.
 * Users don't interact with this directly - use StreamingChunkReader instead.
 */
class StreamingChunkReaderPromise {
private:
    TraceChunk current_chunk;
    std::exception_ptr exception;

public:
    StreamingChunkReaderPromise() = default;

    // Coroutine protocol methods
    StreamingChunkReader get_return_object();
    
    std::suspend_never initial_suspend() { return {}; }
    
    std::suspend_always final_suspend() noexcept { return {}; }
    
    std::suspend_always yield_value(TraceChunk chunk) {
        current_chunk = std::move(chunk);
        return {};
    }
    
    void return_void() {}
    
    void unhandled_exception() {
        exception = std::current_exception();
    }

    // Accessors
    TraceChunk& get_current() { return current_chunk; }
    const TraceChunk& get_current() const { return current_chunk; }
    
    bool has_exception() const { return exception != nullptr; }
    void rethrow_exception() {
        if (exception) std::rethrow_exception(exception);
    }
};

/**
 * @brief Coroutine that streams trace file in fixed-size chunks
 * 
 * Efficiently reads large trace files by yielding data in configurable chunks.
 * Supports C++20 coroutines for lazy evaluation and memory efficiency.
 * 
 * Memory: O(chunk_size) - only current chunk in RAM
 * Typical chunk size: 100 MB
 * For 1TB file: 100 MB peak memory vs 5 TB load-all approach
 * 
 * Usage Example:
 * @code
 * auto reader = create_chunk_reader("trace.vcd", 100 * 1024 * 1024);
 * while (reader) {
 *     TraceChunk chunk = reader.get_chunk();
 *     process_chunk(chunk);
 *     reader.next();  // Yields next chunk via coroutine
 * }
 * @endcode
 * 
 * Thread Safety:
 * NOT thread-safe. Each reader must be used in a single thread.
 * For multi-threaded access, create separate readers for each thread.
 * 
 * Error Handling:
 * File errors are propagated as exceptions when calling next() or get_chunk().
 * 
 * @see SparseIndex for combining with timestamp indexing
 * @see StreamingVCDTrace for trace-format-specific usage
 */
class StreamingChunkReader {
private:
    std::coroutine_handle<StreamingChunkReaderPromise> handle;

    // Private constructor - only used by promise
    explicit StreamingChunkReader(std::coroutine_handle<StreamingChunkReaderPromise> h)
        : handle(h) {}

    friend class StreamingChunkReaderPromise;

public:
    /// Default constructor - creates empty/invalid reader
    StreamingChunkReader() = default;
    
    /// Move constructor
    StreamingChunkReader(StreamingChunkReader&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}
    
    /// Move assignment
    StreamingChunkReader& operator=(StreamingChunkReader&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }
    
    /// Not copyable
    StreamingChunkReader(const StreamingChunkReader&) = delete;
    StreamingChunkReader& operator=(const StreamingChunkReader&) = delete;
    
    /// Destructor - cleans up coroutine
    ~StreamingChunkReader() {
        if (handle) handle.destroy();
    }

    /**
     * @brief Check if reader has more chunks
     * 
     * Returns false when all chunks have been read,
     * or when reader is invalid.
     * 
     * @return true if more chunks are available
     */
    bool has_chunk() const {
        return handle && !handle.done();
    }

    /**
     * @brief Get current chunk without advancing
     * 
     * Call this after is_valid() or after next() to get the chunk data.
     * 
     * @return Reference to current TraceChunk
     * @throws std::logic_error if no chunk available
     * @throws propagated file I/O exceptions
     */
    TraceChunk& get_chunk() {
        if (!handle) {
            throw std::logic_error("Invalid StreamingChunkReader");
        }
        handle.promise().rethrow_exception();
        return handle.promise().get_current();
    }

    const TraceChunk& get_chunk() const {
        if (!handle) {
            throw std::logic_error("Invalid StreamingChunkReader");
        }
        handle.promise().rethrow_exception();
        return handle.promise().get_current();
    }

    /**
     * @brief Advance to next chunk
     * 
     * Resumes the coroutine to read and yield the next chunk.
     * Call get_chunk() after next() to access the data.
     * 
     * @throws propagated file I/O exceptions
     */
    void next() {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }

    /**
     * @brief Implicit boolean conversion
     * 
     * Allows using reader in while() conditions:
     * ```cpp
     * while (reader) {
     *     process(reader.get_chunk());
     *     reader.next();
     * }
     * ```
     * 
     * @return true if reader has chunks available
     */
    explicit operator bool() const {
        return has_chunk();
    }

    // Coroutine promise type
    using promise_type = StreamingChunkReaderPromise;
};

/**
 * @brief Factory function to create a streaming chunk reader
 * 
 * Creates a coroutine-based reader that streams a trace file
 * in fixed-size chunks.
 * 
 * **Performance Tuning:**
 * - Smaller chunks (10-50 MB): Lower memory, more overhead
 * - Larger chunks (100-500 MB): Higher memory, less overhead
 * - Default (100 MB): Good balance for most use cases
 * 
 * @param filename Path to trace file to read
 * @param chunk_size_bytes Size of each chunk in bytes
 *                         (default: 100 MB)
 * @return StreamingChunkReader ready to use
 * @throws std::ios_base::failure if file cannot be opened
 * 
 * @example
 * ```cpp
 * auto reader = create_chunk_reader("huge.vcd", 100 * 1024 * 1024);
 * while (reader) {
 *     const auto& chunk = reader.get_chunk();
 *     std::cout << "Read " << chunk.size() << " bytes at offset "
 *               << chunk.file_offset << std::endl;
 *     reader.next();
 * }
 * ```
 */
StreamingChunkReader create_chunk_reader(const std::string& filename,
                                         size_t chunk_size_bytes = 100 * 1024 * 1024);

/**
 * @brief Internal coroutine implementation (declared, not defined here)
 * 
 * This is the actual coroutine that yields chunks.
 * Defined in streaming_chunk_reader.cpp
 */
StreamingChunkReader chunk_reader_coro(const std::string& filename, size_t chunk_size);

} // namespace wal
