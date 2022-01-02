#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
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

    storage::Chunk ReadFromChunk(std::string video_id, int version, int sequence);

    std::vector<storage::Chunk> WriteToChunk(std::string video_id, int version, std::string data);

    storage::ChunkDeletionIdentifier DeleteChunk(std::string video_id, int version, int sequence);

private:
    std::string Filename(std::string video_id, int version, int sequence);

private:
    std::string address_;
    std::string dir_;
    int max_chunk_size_;
};

}

#endif
