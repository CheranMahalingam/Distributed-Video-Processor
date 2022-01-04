#include "chunk_manager.h"

namespace file_system {

ChunkManager::ChunkManager(std::string address, std::string dir, int chunk_size) 
    : address_(address), dir_(dir), max_chunk_size_(chunk_size) {
    std::filesystem::create_directories(dir);
}

std::vector<storage::Chunk> ChunkManager::CreateChunks(std::string video_id, int version, const std::string& data) {
    std::vector<storage::Chunk> chunks;
    int offset = 0;
    int chunk_count = std::ceil((double)data.size() / (double)max_chunk_size_);
    for (int i = 0; i < chunk_count; i++) {
        std::string chunked_data = data.substr(offset, max_chunk_size_);

        storage::Chunk new_chunk;
        new_chunk.mutable_metadata()->set_videoid(video_id);
        new_chunk.mutable_metadata()->set_version(version);
        new_chunk.mutable_metadata()->set_sequence(i);
        new_chunk.mutable_metadata()->set_last(i == chunk_count - 1);
        new_chunk.set_data(chunked_data);
        chunks.push_back(new_chunk);

        offset += max_chunk_size_;
    }
    return chunks;
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

void ChunkManager::WriteToChunk(const storage::Chunk& chunk) {
    std::string chunk_path = Filename(chunk.metadata().videoid(), chunk.metadata().version(), chunk.metadata().sequence());
    std::ofstream out(chunk_path, std::ios::binary);
    SerializeDelimitedToOstream(chunk, &out);

    logger(LogLevel::Debug) << "Chunk written to" << chunk_path;
    out.close();
}

void ChunkManager::DeleteChunks(std::string video_id, int version) {
    int file_count = ChunkCount(video_id, version);
    for (int i = 0; i < file_count; i++) {
        std::string chunk_path = Filename(video_id, version, i);
        DeleteFile(chunk_path);
    }
}

void ChunkManager::DeleteFile(std::string path) {
    if (std::filesystem::remove(path)) {
        logger(LogLevel::Debug) << path << "chunk deleted";
    } else {
        logger(LogLevel::Info) << "Unable to find chunk" << path;
    }
}

std::string ChunkManager::Filename(std::string video_id, int version, int sequence) {
    return dir_ + "/" + address_ + "/" + video_id + "/" + std::to_string(version) + "/chunk"  + std::to_string(sequence);
}

int ChunkManager::ChunkCount(std::string video_id, int version) {
    bool last = false;
    int file_count = 0;
    while (!last) {
        auto chunk = ReadFromChunk(video_id, version, file_count);
        last = chunk.metadata().last();
        file_count++;
    }
    return file_count;
}

}
