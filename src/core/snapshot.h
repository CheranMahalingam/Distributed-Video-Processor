#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <string>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <vector>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>

#include "log.h"
#include "raft.grpc.pb.h"

namespace raft {

using google::protobuf::util::SerializeDelimitedToOstream;
using google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using google::protobuf::io::IstreamInputStream;

class Snapshot {
public:
    Snapshot(const std::string dir);

    void PersistState(const int term, const std::string vote_id);

    void PersistLog(const std::vector<rpc::LogEntry>& entries, const bool append);

    std::tuple<int, std::string> RestoreState();

    std::vector<rpc::LogEntry> RestoreLog();

    bool Empty() const;

private:
    std::string log_file_;
    std::string state_file_;
};

}

#endif
