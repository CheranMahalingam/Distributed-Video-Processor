#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <cmath>
#include <google/protobuf/util/delimited_message_util.h>

#include "log.h"
#include "file_system.grpc.pb.h"

namespace file_system {

using google::protobuf::util::SerializeDelimitedToOstream;
using google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using google::protobuf::io::IstreamInputStream;

class ChunkManager {
public:
    ChunkManager(std::string address, std::string dir, int chunk_size);

    std::vector<storage::Chunk> CreateChunks(std::string video_id, int version, const std::string& data);

    storage::Chunk ReadFromChunk(std::string video_id, int version, int sequence);

    void WriteToChunk(const storage::Chunk& chunk);

    void DeleteChunks(std::string video_id, int version);

private:
    std::string Filename(std::string video_id, int version, int sequence);

    void DeleteFile(std::string path);

    int ChunkCount(std::string video_id, int version);

private:
    std::string address_;
    std::string dir_;
    int max_chunk_size_;
};

}

#endif
