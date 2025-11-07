#include "../../include/wal/trace/streaming_chunk_reader.h"
#include <fstream>
#include <stdexcept>

namespace wal {

// Promise implementation
StreamingChunkReader StreamingChunkReaderPromise::get_return_object() {
    return StreamingChunkReader(
        std::coroutine_handle<StreamingChunkReaderPromise>::from_promise(*this)
    );
}

// Coroutine implementation - the actual streaming logic
StreamingChunkReader chunk_reader_coro(const std::string& filename, size_t chunk_size) {
    // Open file in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::ios_base::failure("Cannot open file: " + filename);
    }

    // Get file size
    file.seekg(0, std::ios::end);
    int64_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size == 0) {
        throw std::ios_base::failure("File is empty: " + filename);
    }

    // Read and yield chunks
    std::vector<char> buffer(chunk_size);
    int64_t offset = 0;

    while (offset < file_size) {
        // Read chunk
        size_t bytes_to_read = std::min(static_cast<int64_t>(chunk_size), 
                                         file_size - offset);
        
        file.read(buffer.data(), bytes_to_read);
        
        if (!file && !file.eof()) {
            throw std::ios_base::failure("Error reading file: " + filename);
        }

        // Create chunk
        TraceChunk chunk;
        chunk.file_offset = offset;
        chunk.data.assign(buffer.data(), buffer.data() + bytes_to_read);
        chunk.is_final_chunk = (offset + bytes_to_read >= file_size);

        // Yield chunk
        co_yield chunk;

        offset += bytes_to_read;
    }
}

// Factory function
StreamingChunkReader create_chunk_reader(const std::string& filename,
                                         size_t chunk_size_bytes) {
    if (chunk_size_bytes == 0) {
        throw std::invalid_argument("chunk_size_bytes must be > 0");
    }
    
    return chunk_reader_coro(filename, chunk_size_bytes);
}

} // namespace wal
