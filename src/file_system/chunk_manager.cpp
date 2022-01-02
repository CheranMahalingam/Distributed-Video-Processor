#include "chunk_manager.h"

namespace file_system {

ChunkManager::ChunkManager(std::string address, std::string dir, int chunk_size) : address_(address), dir_(dir), max_chunk_size_(chunk_size) {
    std::filesystem::create_directories(dir);
}

storage::Chunk ChunkManager::ReadFromChunk(std::string video_id, int version, int sequence) {
    std::string chunk_path = Filename(video_id, version, sequence);
    std::ifstream in(chunk_path, std::ios::binary);
    IstreamInputStream chunk_stream(&in);

    storage::Chunk chunk;
    ParseDelimitedFromZeroCopyStream(&chunk, &chunk_stream, nullptr);

    logger(LogLevel::Debug) << "Chunk read from" << chunk_path;
    in.close();

    return chunk;
}

std::vector<storage::Chunk> ChunkManager::WriteToChunk(std::string video_id, int version, std::string data) {
    std::vector<storage::Chunk> chunks;
    int offset = 0;
    int chunk_count = data.size() / max_chunk_size_ + 1;
    for (int i = 0; i < chunk_count; i++) {
        std::string chunk_path = Filename(video_id, version, i);
        std::ofstream out(chunk_path, std::ios::binary);
        std::string chunked_data = data.substr(offset, max_chunk_size_);

        storage::Chunk new_chunk;
        new_chunk.mutable_metadata()->set_videoid(video_id);
        new_chunk.mutable_metadata()->set_version(version);
        new_chunk.mutable_metadata()->set_sequence(i);
        new_chunk.mutable_metadata()->set_last(i == chunk_count - 1);
        new_chunk.set_data(chunked_data);
        chunks.push_back(new_chunk);

        SerializeDelimitedToOstream(new_chunk, &out);

        logger(LogLevel::Debug) << "Chunk written to" << chunk_path;
        offset += max_chunk_size_;
        out.close();
    }

    return chunks;
}

storage::ChunkDeletionIdentifier ChunkManager::DeleteChunk(std::string video_id, int version, int sequence) {
    std::string chunk_path = Filename(video_id, version, sequence);
    if (std::filesystem::remove(chunk_path)) {
        logger(LogLevel::Debug) << chunk_path << "chunk deleted";
    } else {
        logger(LogLevel::Info) << "Unable to find chunk" << chunk_path;
    }

    storage::ChunkDeletionIdentifier id;
    id.set_path(chunk_path);
    return id;
}

std::string ChunkManager::Filename(std::string video_id, int version, int sequence) {
    return dir_ + "/" + address_ + "/" + video_id + "/" + std::to_string(version) + "/chunk"  + std::to_string(sequence);
}

}
